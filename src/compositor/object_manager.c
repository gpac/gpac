/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

GF_EXPORT
GF_ObjectManager *gf_odm_new()
{
	GF_ObjectManager *tmp;
	GF_SAFEALLOC(tmp, GF_ObjectManager);
	if (!tmp) return NULL;

#ifndef GPAC_DISABLE_VRML
	tmp->ms_stack = gf_list_new();
	tmp->mc_stack = gf_list_new();
#endif
	return tmp;
}

void gf_odm_reset_media_control(GF_ObjectManager *odm, Bool signal_reset)
{
#ifndef GPAC_DISABLE_VRML
	MediaSensorStack *media_sens;
	MediaControlStack *media_ctrl;

	while ((media_sens = (MediaSensorStack *)gf_list_last(odm->ms_stack))) {
		MS_Stop(media_sens);
		/*and detach from stream object*/
		media_sens->stream = NULL;
		gf_list_rem_last(odm->ms_stack);
	}

	while ((media_ctrl = (MediaControlStack *)gf_list_last(odm->mc_stack))) {
		if (signal_reset)
			gf_odm_remove_mediacontrol(odm, media_ctrl);
		media_ctrl->stream = NULL;
		media_ctrl->ck = NULL;
		gf_list_rem_last(odm->mc_stack);
	}
#endif
}


void gf_odm_del(GF_ObjectManager *odm)
{
	if (odm->addon && (odm->addon->root_od==odm)) {
		odm->addon->root_od = NULL;
		odm->addon->started = 0;
	}
	if (odm->upper_layer_odm) {
		odm->upper_layer_odm->lower_layer_odm = NULL;
	}
	if (odm->lower_layer_odm) {
		odm->lower_layer_odm->upper_layer_odm = NULL;
	}

	/*detach media object as referenced by the scene - this should ensures that any attempt to lock the ODM from the
	compositor will fail as the media object is no longer linked to object manager*/
	if (odm->mo) odm->mo->odm = NULL;

#ifndef GPAC_DISABLE_VRML
	gf_odm_reset_media_control(odm, 0);
	gf_list_del(odm->ms_stack);
	gf_list_del(odm->mc_stack);
#endif

	if (odm->type == GF_STREAM_INTERACT)
		gf_input_sensor_delete(odm);

	if (odm->raw_frame_sema) gf_sema_del(odm->raw_frame_sema);

	if (odm->pid) gf_filter_pid_set_udta(odm->pid, NULL);
	if (odm->extra_pids) {
		while (gf_list_count(odm->extra_pids)) {
			GF_ODMExtraPid *xpid = gf_list_pop_back(odm->extra_pids);
			if (xpid->pid) gf_filter_pid_set_udta(xpid->pid, NULL);
			gf_free(xpid);
		}
		gf_list_del(odm->extra_pids);
	}
	gf_free(odm);
}


void gf_odm_register_pid(GF_ObjectManager *odm, GF_FilterPid *pid, Bool register_only)
{
	u32 es_id=0;
	const GF_PropertyValue *prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (!prop) prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (prop) es_id = prop->value.uint;

	if (! odm->pid) {
		odm->pid = pid;
		odm->pid_id = es_id;
	} else {
		GF_ODMExtraPid *xpid;
		if (!odm->extra_pids) odm->extra_pids = gf_list_new();
		GF_SAFEALLOC(xpid, GF_ODMExtraPid);
		xpid->pid = pid;
		xpid->pid_id = es_id;
		gf_list_add(odm->extra_pids, xpid);
	}

	if (register_only) return;

	gf_odm_setup_object(odm, odm->subscene ? odm->scene_ns : odm->parentscene->root_od->scene_ns, pid);
}

//this function is not exposed and shall only be used for the reset scene event
//it sends the event synchronously, as needed for gf_term_disconnect()
void gf_filter_pid_exec_event(GF_FilterPid *pid, GF_FilterEvent *evt);

GF_EXPORT
void gf_odm_disconnect(GF_ObjectManager *odm, u32 do_remove)
{
	GF_Compositor *compositor = odm->parentscene ? odm->parentscene->compositor : odm->subscene->compositor;

	gf_odm_stop(odm, GF_TRUE);

	/*disconnect sub-scene*/
	if (odm->subscene) {
		//send a scene reset
		if (odm->pid) {
			GF_FilterEvent fevt;

			GF_FEVT_INIT(fevt, GF_FEVT_RESET_SCENE, odm->pid);
			fevt.attach_scene.object_manager = odm;
			gf_filter_pid_exec_event(odm->pid, &fevt);
		}
		gf_scene_disconnect(odm->subscene, do_remove ? GF_TRUE : GF_FALSE);
	}
	/*no destroy*/
	if (!do_remove) return;

	/*unload the decoders before deleting the channels to prevent any access fault*/
	if (odm->type==GF_STREAM_INTERACT) {
		u32 i, count;
		// Remove all Registered InputSensor nodes -> shut down the InputSensor threads -> prevent illegal access on deleted pointers
		GF_MediaObject *obj = odm->mo;
		count = gf_mo_event_target_count(obj);
		for (i=0; i<count; i++) {
			GF_Node *n = (GF_Node *)gf_event_target_get_node(gf_mo_event_target_get(obj, i));
			switch (gf_node_get_tag(n)) {
#ifndef GPAC_DISABLE_VRML
			case TAG_MPEG4_InputSensor:
				((M_InputSensor*)n)->enabled = 0;
				InputSensorModified(n);
				break;
#endif
			default:
				break;
			}
		}
	}

	/*detach from network service */
	if (odm->scene_ns) {
		GF_Scene *scene = odm->parentscene;
		GF_SceneNamespace *ns = odm->scene_ns;
		if (ns->nb_odm_users) ns->nb_odm_users--;
		if (ns->owner == odm) {
			/*detach it!!*/
			ns->owner = NULL;
			/*try to assign a new root in case this is not scene shutdown*/
			if (ns->nb_odm_users && odm->parentscene) {
				GF_ObjectManager *new_root;
				u32 i = 0;
				while ((new_root = (GF_ObjectManager *)gf_list_enum(odm->parentscene->resources, &i)) ) {
					if (new_root == odm) continue;
					if (new_root->scene_ns != ns) continue;

					ns->owner = new_root;
					break;
				}
			}
		} else {
			assert(ns->nb_odm_users);
		}
		scene = scene ? gf_scene_get_root_scene(scene) : NULL;
		odm->scene_ns = NULL;
		if (!ns->nb_odm_users && scene) gf_scene_ns_del(ns, scene);
	}


	/*delete from the parent scene.*/
	if (odm->parentscene) {
		GF_Event evt;

		if (odm->addon) {
			gf_list_del_item(odm->parentscene->declared_addons, odm->addon);
			gf_scene_reset_addon(odm->addon, GF_FALSE);
			odm->addon = NULL;
		}

		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_FALSE;
		gf_fs_forward_event(odm->parentscene->compositor->fsess, &evt, GF_FALSE, GF_TRUE);

		gf_scene_remove_object(odm->parentscene, odm, do_remove);
		if (odm->subscene) gf_scene_del(odm->subscene);
		gf_odm_del(odm);
		return;
	}

	/*this is the scene root OD (may be a remote OD ..) */
	if (odm->subscene) {
		GF_Event evt;

		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_FALSE;
		gf_sc_send_event(compositor, &evt);
		gf_scene_del(odm->subscene);
	}

	/*delete the ODMan*/
	gf_odm_del(odm);
}

static Bool gf_odm_should_auto_select(GF_ObjectManager *odm)
{
	u32 i, count;
	if (odm->type == GF_STREAM_SCENE) return GF_TRUE;
	//TODO- detect image media to start playback right away
	//if (odm->type == GF_STREAM_VISUAL) return GF_TRUE;

	if (odm->parentscene && !odm->parentscene->is_dynamic_scene) {
		return GF_TRUE;
	}

	if (odm->parentscene && odm->parentscene->root_od->addon) {
		if (odm->parentscene->root_od->addon->addon_type == GF_ADDON_TYPE_MAIN)
			return GF_FALSE;
	}

	count = gf_list_count(odm->parentscene->resources);
	for (i=0; i<count; i++) {
		GF_ObjectManager *an_odm = gf_list_get(odm->parentscene->resources, i);
		if (an_odm==odm) continue;
		if (an_odm->type != odm->type) continue;
		//same type - if the first one has been autumatically activated, do not activate this one
		if (an_odm->state == GF_ODM_STATE_PLAY) return GF_FALSE;
	}
	return GF_TRUE;
}


void gf_odm_setup_remote_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, char *remote_url)
{
	char *parent_url = NULL;
	if (!remote_url) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] No URL specified for remote object - ignoring object setup\n", odm->ID));
		return;
	}

	if (!odm->scene_ns) {
		if (odm->flags & GF_ODM_DESTROYED) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Object has been scheduled for destruction - ignoring object setup\n", odm->ID));
			return;
		}
		odm->scene_ns = parent_ns;
		odm->scene_ns->nb_odm_users++;
	}

	/*store original OD ID */
	if (!odm->media_current_time) odm->media_current_time = odm->ID;

	//detach it
	odm->scene_ns = NULL;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Object redirection to %s (MO %08x)\n", odm->ID, remote_url, odm->mo));

	/*if object is a scene, create the inline before connecting the object.
		This is needed in irder to register the nodes using the resource for event
		propagation (stored at the inline level)
	*/
	if (odm->mo && (odm->mo->type==GF_MEDIA_OBJECT_SCENE)) {
		odm->subscene = gf_scene_new(NULL, odm->parentscene);
		odm->subscene->root_od = odm;
		//scenes are by default dynamic
		odm->subscene->is_dynamic_scene = GF_TRUE;
	}
	parent_url = parent_ns ? parent_ns->url : NULL;
	if (parent_url && !strnicmp(parent_url, "views://", 8))
		parent_url = NULL;

	//make sure we don't have an ID before attempting to connect
	if (odm->ID == GF_MEDIA_EXTERNAL_ID) odm->ID = 0;
	odm->ServiceID = 0;
	gf_scene_ns_connect_object(odm->subscene ? odm->subscene : odm->parentscene, odm, remote_url, parent_url);
}

GF_EXPORT
void gf_odm_setup_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, GF_FilterPid *for_pid)
{
	GF_Err e;
	GF_MediaObject *syncRef;

	if (!odm->scene_ns) {
		if (odm->flags & GF_ODM_DESTROYED) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Object has been scheduled for destruction - ignoring object setup\n", odm->ID));
			return;
		}
		odm->scene_ns = parent_ns;
		odm->scene_ns->nb_odm_users++;
	}

	/*restore OD ID */
	if (odm->media_current_time) {
		odm->ID = odm->media_current_time;
		odm->media_current_time = 0;
		odm->flags |= GF_ODM_REMOTE_OD;
	}

	syncRef = (GF_MediaObject*)odm->sync_ref;

	if (odm->scene_ns->owner &&  (odm->scene_ns->owner->flags & GF_ODM_INHERIT_TIMELINE)) {
		odm->flags |= GF_ODM_INHERIT_TIMELINE;
	}

	/*empty object, use a dynamic scene*/
	if (! odm->pid && odm->subscene) {
		assert(odm->subscene->root_od==odm);
		odm->subscene->is_dynamic_scene = GF_TRUE;
	} else if (odm->pid) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Setting up object streams\n"));

		e = gf_odm_setup_pid(odm, for_pid);
		if (e) {
			GF_LOG(GF_LOG_MEDIA, GF_LOG_ERROR, ("Service %s PID %s Setup Failure: %s", odm->scene_ns->url, gf_filter_pid_get_name(for_pid ? for_pid : odm->pid), gf_error_to_string(e) ));
		}
	}

	if (odm->pid && !odm->buffer_playout_us) {
		GF_FilterEvent evt;
		const GF_PropertyValue *prop;
		GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
		const char *opt;

		opt = gf_cfg_get_key(scene->compositor->user->config, "Network", "BufferLength");
		if (opt) odm->buffer_playout_us = (u32) 1000*atof(opt);
		else odm->buffer_playout_us = 3000000;

		opt = gf_cfg_get_key(scene->compositor->user->config, "Network", "RebufferLength");
		if (opt) odm->buffer_min_us = (u32) 1000*atof(opt);
		else odm->buffer_min_us = 0;

		opt = gf_cfg_get_key(scene->compositor->user->config, "Network", "BufferMaxOccupancy");
		if (opt) odm->buffer_max_us = (u32) 1000*atof(opt);
		else odm->buffer_max_us = odm->buffer_playout_us;

		//check the same on the pid
		prop = gf_filter_pid_get_property_str(for_pid ? for_pid : odm->pid, "BufferLength");
		if (prop) odm->buffer_playout_us = prop->value.uint;
		prop = gf_filter_pid_get_property_str(for_pid ? for_pid : odm->pid, "RebufferLength");
		if (prop) odm->buffer_min_us = prop->value.uint;
		prop = gf_filter_pid_get_property_str(for_pid ? for_pid : odm->pid, "BufferMaxOccupancy");
		if (prop) odm->buffer_max_us = prop->value.uint;

		prop = gf_filter_pid_get_info(for_pid ? for_pid : odm->pid, GF_PROP_PID_FILE_CACHED);
		if (prop) {
			odm->buffer_playout_us = odm->buffer_max_us = 1000000;
			odm->buffer_min_us = 0;
		}
		GF_FEVT_INIT(evt, GF_FEVT_BUFFER_REQ, for_pid ? for_pid : odm->pid);
		evt.buffer_req.max_buffer_us = odm->buffer_max_us;
		gf_filter_pid_send_event(NULL, &evt);
	}


	/*setup mediaobject info except for top-level OD*/
	if (odm->parentscene) {
		GF_Event evt;

		//this may result in an attempt to lock the compositor, so release the net MX before
		if (!odm->scalable_addon) {
			gf_scene_setup_object(odm->parentscene, odm);
		}

		/*setup node decoder*/
#if FILTER_FIXME
		if (odm->mo && odm->codec && odm->codec->decio && (odm->codec->decio->InterfaceType==GF_NODE_DECODER_INTERFACE) ) {
			GF_NodeDecoder *ndec = (GF_NodeDecoder *) odm->codec->decio;
			GF_Node *n = gf_event_target_get_node(gf_mo_event_target_get(odm->mo, 0));
			if (n) ndec->AttachNode(ndec, n);

			/*not clear in the spec how the streams attached to AFX are started - default to "right now"*/
			gf_odm_start(odm);
		}
#endif
		if (odm->pid && (odm->pid==for_pid)) {
			evt.type = GF_EVENT_CONNECT;
			evt.connect.is_connected = GF_TRUE;
			gf_fs_forward_event(odm->parentscene->compositor->fsess, &evt, GF_FALSE, GF_TRUE);
		}
	} else if (odm->pid==for_pid) {
		/*othewise send a connect ack for top level*/
		GF_Event evt;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Root object connected (%s) !\n", odm->scene_ns->url));

		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_TRUE;
		gf_sc_send_event(odm->subscene->compositor, &evt);
	}


	/* start object*/
	/*object is already started (new PID was inserted for this object)*/
	if (odm->state==GF_ODM_STATE_PLAY) {
		if (odm->pid==for_pid)
			odm->state = GF_ODM_STATE_STOP;
		gf_odm_start(odm);
	}
	/*object is the root, always start*/
	else if (!odm->parentscene) {
		assert(odm->subscene && (odm->subscene->root_od==odm));
		odm->flags &= ~GF_ODM_NOT_SETUP;
		gf_odm_start(odm);
	}
	/*object is a pure OCR object - connect*/
	else if (odm->type==GF_STREAM_OCR) {
		odm->flags &= ~GF_ODM_NOT_SETUP;
		gf_odm_start(odm);
	}
	/*if the object is inserted from a broadcast, start it if not already done. This covers cases where the scene (BIFS, LASeR) and
	the media (images) are both carrouseled and the carrousels are interleaved. If we wait for the scene to trigger a PLAY, we will likely
	have to wait for an entire image carousel period to start filling the buffers, which is sub-optimal
	we also force a prefetch for object declared outside the OD stream to make sure we don't loose any data before object declaration and play
	as can be the case with MPEG2 TS (first video packet right after the PMT) - this should be refined*/
	else if ( ((odm->flags & GF_ODM_NO_TIME_CTRL) || (odm->flags & GF_ODM_NOT_IN_OD_STREAM)) && gf_odm_should_auto_select(odm) && (odm->parentscene->selected_service_id == odm->ServiceID)) {
		Bool force_play = GF_FALSE;

		if (odm->state==GF_ODM_STATE_STOP) {
			odm->flags |= GF_ODM_PREFETCH;
			force_play = GF_TRUE;
		}

		if (force_play) {
			odm->flags |= GF_ODM_INITIAL_BROADCAST_PLAY;
			odm->parentscene->selected_service_id = odm->ServiceID;

			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] Inserted from input service %s - forcing play\n", odm->ID, odm->scene_ns->url));
			odm->flags &= ~GF_ODM_NOT_SETUP;
			gf_odm_start(odm);
		}
	}

	odm->flags &= ~GF_ODM_NOT_SETUP;

	/*for objects inserted by user (subs & co), auto select*/
	if (odm->parentscene && odm->parentscene->is_dynamic_scene
	        && (odm->ID==GF_MEDIA_EXTERNAL_ID)
	        && (odm->flags & GF_ODM_REMOTE_OD)
	   ) {
		GF_Event evt;

		if (odm->addon) {
			Bool role_set = 0;

			if (odm->addon->addon_type >= GF_ADDON_TYPE_MAIN) return;

			//check role - for now look into URL, we need to inspect DASH roles
			if (odm->mo && odm->mo->URLs.count && odm->mo->URLs.vals[0].url) {
				char *sep = strchr(odm->mo->URLs.vals[0].url, '?');
				if (sep && strstr(sep, "role=main")) {
					odm->addon->addon_type = GF_ADDON_TYPE_MAIN;
					role_set = 1;
				}
			}

			if (!role_set) {
				const GF_PropertyValue *prop = gf_filter_pid_get_property_str(for_pid ? for_pid : odm->pid, "role");
				if (prop && prop->value.string && !strcmp(prop->value.string, "main")) {
					odm->addon->addon_type = GF_ADDON_TYPE_MAIN;
				}
			}


			if (odm->addon->addon_type == GF_ADDON_TYPE_ADDITIONAL) {
				gf_scene_select_object(odm->parentscene, odm);
			}
			return;
		}

		if (!odm->parentscene) {
			evt.type = GF_EVENT_STREAMLIST;
			gf_sc_send_event(odm->parentscene->compositor, &evt);
			return;
		}
	}
}

/*setup channel, clock and query caps*/
GF_EXPORT
GF_Err gf_odm_setup_pid(GF_ObjectManager *odm, GF_FilterPid *pid)
{
	GF_Clock *ck;
	GF_List *ck_namespace;
	s8 flag;
	u16 clockID;
	GF_Scene *scene;
	Bool clock_inherited = GF_TRUE;
	const GF_PropertyValue *prop;
	u32 OD_OCR_ID=0;
	u32 es_id=0;

	/*find the clock for this new channel*/
	ck = NULL;
	flag = (s8) -1;
	if (!pid) pid = odm->pid;

	if (odm->ck) {
		ck = odm->ck;
		goto clock_setup;
	}

	/*sync reference*/
	if (odm->sync_ref && odm->sync_ref->odm) {
		ck = odm->sync_ref->odm->ck;
		goto clock_setup;
	}
	/*timeline override*/
	if (odm->flags & GF_ODM_INHERIT_TIMELINE) {
		ck = odm->parentscene->root_od->ck;

		/**/
		if (!ck) {
			GF_ObjectManager *odm_par = odm->parentscene->root_od->parentscene->root_od;
			while (odm_par) {
				ck = odm_par->ck;

				if (ck) break;

				odm_par = (odm_par->parentscene && odm_par->parentscene->root_od->parentscene ) ? odm_par->parentscene->root_od->parentscene->root_od : NULL;
			}
		}
		if (ck)
			goto clock_setup;
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM] Cannot inherit timeline from parent scene for scene %s - new clock will be created\n", odm->scene_ns->url));
	}

	/*get clocks namespace (eg, parent scene)*/
	scene = odm->subscene ? odm->subscene : odm->parentscene;
	if (!scene) return GF_BAD_PARAM;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CLOCK_ID);
	if (prop) OD_OCR_ID = prop->value.uint;

	ck_namespace = odm->scene_ns->Clocks;
	odm->set_speed = odm->scene_ns->set_speed;

	/*little trick for non-OD addressing: if object is a remote one, and service owner already has clocks,
	override OCR. This will solve addressing like file.avi#audio and file.avi#video*/
	if (!OD_OCR_ID && (odm->flags & GF_ODM_REMOTE_OD) && (gf_list_count(ck_namespace)==1) ) {
		ck = (GF_Clock*)gf_list_get(ck_namespace, 0);
		OD_OCR_ID = ck->clock_id;
	}
	/*for dynamic scene, force all streams to be sync on main OD stream (one timeline, no need to reload ressources)*/
	else if (odm->parentscene && odm->parentscene->is_dynamic_scene && !odm->subscene) {
		GF_ObjectManager *parent_od = odm->parentscene->root_od;
		if (parent_od->scene_ns && (gf_list_count(parent_od->scene_ns->Clocks)==1)) {
			ck = (GF_Clock*)gf_list_get(parent_od->scene_ns->Clocks, 0);
			if (!odm->ServiceID || (odm->ServiceID==ck->service_id)) {
				OD_OCR_ID = ck->clock_id;
				goto clock_setup;
			}
		}
	}

	/*do we have an OCR specified*/
	clockID = OD_OCR_ID;
	/*if OCR stream force self-synchro !!*/
	if (odm->type == GF_STREAM_OCR) clockID = odm->ID;
	if (!clockID) {
		if (odm->ID == GF_MEDIA_EXTERNAL_ID) {
			clockID = (u32) odm->scene_ns;
		} else {
			clockID = odm->ID;
		}
	}

	/*override clock dependencies if specified*/
	if (scene->compositor->force_single_clock) {
		GF_Scene *parent = gf_scene_get_root_scene(scene);
		clockID = scene->root_od->ck->clock_id;
		ck_namespace = parent->root_od->scene_ns->Clocks;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (!prop) prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (prop) es_id = prop->value.uint;

	ck = gf_clock_attach(ck_namespace, scene, clockID, es_id, flag);
	if (!ck) return GF_OUT_OF_MEM;

	ck->service_id = odm->ServiceID;
	clock_inherited = GF_FALSE;

	if (es_id==ck->clock_id)
		odm->owns_clock = GF_TRUE;

	if (scene->root_od->subscene && scene->root_od->subscene->is_dynamic_scene && !scene->root_od->ck)
		scene->root_od->ck = ck;

clock_setup:
	assert(ck);
	odm->ck = ck;
	odm->clock_inherited = clock_inherited;

	gf_odm_update_duration(odm, pid);
	/*regular setup*/
	return GF_OK;
}

void gf_odm_update_duration(GF_ObjectManager *odm, GF_FilterPid *pid)
{
	u64 dur=0;
	const GF_PropertyValue *prop;
	prop = gf_filter_pid_get_info(pid, GF_PROP_PID_DURATION);
	if (prop) {
		dur = 1000 * prop->value.frac.num;
		if (prop->value.frac.den) dur /= prop->value.frac.den;
	}
	if ((u32) dur > odm->duration) {
		odm->duration = (u32) dur;
		/*update scene duration*/
		gf_scene_set_duration(odm->subscene ? odm->subscene : odm->parentscene);
	}
}

/*this is the tricky part: make sure the net is locked before doing anything since an async service
reply could destroy the object we're queuing for play*/
void gf_odm_start(GF_ObjectManager *odm)
{
	/*object is not started - issue channel setup requests*/
	if (!odm->state) {
		/*look for a given segment name to play*/
		if (odm->subscene) {
			char *url, *frag=NULL;
			assert(odm->subscene->root_od==odm);

			url = (odm->mo && odm->mo->URLs.count) ? odm->mo->URLs.vals[0].url : odm->scene_ns->url;
			frag = strrchr(url, '#');

			if (frag) {
				GF_Segment *seg = gf_odm_find_segment(odm, frag+1);
				if (seg) {
					odm->media_start_time = (u64) ((s64) seg->startTime*1000);
					odm->media_stop_time =  (u64) ((s64) (seg->startTime + seg->Duration)*1000);
				}
			}
		}
	}
	gf_odm_play(odm);

}

void gf_odm_play(GF_ObjectManager *odm)
{
	u64 range_end;
	Bool media_control_paused = 0;
	Bool start_range_is_clock = 0;
	Double ck_time;
	GF_Clock *clock = odm->ck;
	GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
#endif
	GF_FilterEvent com;
	GF_Clock *parent_ck = NULL;

	if (odm->mo && odm->mo->pck && !(odm->flags & GF_ODM_PREFETCH)) {
		/*reset*/
		gf_filter_pck_unref(odm->mo->pck);
		odm->mo->pck = NULL;
	}

	if (odm->parentscene) {
		parent_ck = gf_odm_get_media_clock(odm->parentscene->root_od);
		if (!gf_odm_shares_clock(odm, parent_ck)) parent_ck = NULL;
	}

	//PID not yet attached, mark as state_play and wait for error or OK
	if (!odm->pid ) {
		odm->state = GF_ODM_STATE_PLAY;
		return;
	}

	if ( !(odm->flags & GF_ODM_INHERIT_TIMELINE) && odm->owns_clock ) {
		gf_clock_reset(clock);
	}

	range_end = odm->media_stop_time;

	/*send play command*/
	GF_FEVT_INIT(com, GF_FEVT_PLAY, odm->pid)

	if (odm->flags & GF_ODM_INITIAL_BROADCAST_PLAY) {
		odm->flags &= ~GF_ODM_INITIAL_BROADCAST_PLAY;
		com.play.initial_broadcast_play = 1;
	}

	/*play from requested time (seeking or non-mpeg4 media control)*/
	if (odm->media_start_time && !clock->clock_init) {
		ck_time = (Double) (s64) odm->media_start_time;
		ck_time /= 1000;
	}
	else if (odm->parentscene && odm->parentscene->root_od->media_start_time && !clock->clock_init) {
		ck_time = (Double) (s64) odm->parentscene->root_od->media_start_time;
		ck_time /= 1000;
	}
	/*play from current time*/
	else {
		ck_time = gf_clock_media_time(clock);
		ck_time /= 1000;
		start_range_is_clock = 1;
	}

	/*handle initial start - MPEG-4 is a bit annoying here, streams are not started through OD but through
	scene nodes. If the stream runs on the BIFS/OD clock, the clock is already started at this point and we're
	sure to get at least a one-frame delay in PLAY, so just remove it - note we're generous but this shouldn't hurt*/
	if (ck_time<=0.5) ck_time = 0;


	/*adjust time for addons*/
	if (odm->parentscene && odm->parentscene->root_od->addon) {
		com.play.initial_broadcast_play = 0;
		//addon timing is resolved against timestamps, not media time
		if (start_range_is_clock) {
			if (!gf_clock_is_started(clock)) {
				ck_time = (Double) odm->parentscene->root_od->addon->media_pts;
				ck_time /= 90000;
			} else {
				ck_time = gf_clock_time(clock);
				ck_time /= 1000;
			}
		}
		ck_time = gf_scene_adjust_time_for_addon(odm->parentscene->root_od->addon, ck_time, &com.play.timestamp_based);
		//we are having a play request for an addon without the main content being active - we no longer have timestamp info from the main content
		if (!clock->clock_init && com.play.timestamp_based)
			com.play.timestamp_based = 2;

		if (ck_time<0)
			ck_time=0;

		if (odm->scalable_addon) {
			//this is a scalable extension to an object in the parent scene
			gf_scene_select_scalable_addon(odm->parentscene->root_od->parentscene, odm);
		}
	}

	com.play.start_range = ck_time;

	if (range_end) {
		com.play.end_range = (s64) range_end / 1000.0;
	} else {
		if (!odm->subscene && odm->parentscene && gf_odm_shares_clock(odm->parentscene->root_od, clock)
				&& (odm->parentscene->root_od->media_stop_time != odm->parentscene->root_od->duration)
		) {
			com.play.end_range = (s64) odm->parentscene->root_od->media_stop_time / 1000.0;
		} else {
			com.play.end_range = 0;
		}
	}

	com.play.speed = clock->speed;

#ifndef GPAC_DISABLE_VRML
	ctrl = parent_ck ? parent_ck->mc : gf_odm_get_mediacontrol(odm);
	/*override range and speed with MC*/
	if (ctrl && !odm->disable_buffer_at_next_play) {
		//for addon, use current clock settings (media control is ignored)
		if (!odm->parentscene || !odm->parentscene->root_od->addon) {
			//this is fake timeshift, eg we are playing a VoD as a timeshift service: stop and start times have already been adjusted
			if (ctrl->control->mediaStopTime<0 && !odm->timeshift_depth) {
			} else {
				MC_GetRange(ctrl, &com.play.start_range, &com.play.end_range);
			}
		}

		com.play.speed = FIX2FLT(ctrl->control->mediaSpeed);
		/*if the channel doesn't control the clock, jump to current time in the controled range, not just the beginning*/
		if ((ctrl->stream != odm->mo) && (ck_time>com.play.start_range) && (com.play.end_range>com.play.start_range)
		&& (ck_time<com.play.end_range)) {
			com.play.start_range = ck_time;
		}
		if (ctrl->paused) media_control_paused = 1;

		gf_clock_set_speed(clock, ctrl->control->mediaSpeed);
		if (odm->mo) odm->mo->speed = ctrl->control->mediaSpeed;

#if 0
		/*if requested seek time AND media control, adjust start range to current play time*/
		if ((com.play.speed>=0) && odm->media_start_time) {
			if ((com.play.start_range>=0) && (com.play.end_range>com.play.start_range)) {
				if (ctrl->control->loop) {
					Double active_dur = com.play.end_range - com.play.start_range;
					while (ck_time>active_dur) ck_time -= active_dur;
				} else {
					ck_time = 0;
					//com.play.start_range = com.play.end_range;
				}
			}
			com.play.start_range += ck_time;
		}
#endif

	}
#endif //GPAC_DISABLE_VRML

	/*full object playback*/
	if (com.play.end_range<=0) {
		if (com.play.speed<0) {
			odm->media_stop_time = 0;
		} else {
			odm->media_stop_time = odm->subscene ? 0 : odm->duration;
		}
	} else {
		/*segment playback - since our timing is in ms whereas segment ranges are double precision,
		make sure we have a LARGER range in ms, otherwise media sensors won't deactivate properly*/
		odm->media_stop_time = (u64) ceil(1000 * com.play.end_range);
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] PID %s: At OTB %u requesting PLAY from %g to %g (clock init %d) - speed %g\n", odm->ID, odm->scene_ns->url, gf_filter_pid_get_name(odm->pid), gf_clock_time(clock), com.play.start_range, com.play.end_range, clock->clock_init, com.play.speed));


	if (odm->state != GF_ODM_STATE_PLAY) {
		odm->state = GF_ODM_STATE_PLAY;
		odm->nb_buffering ++;
		scene->nb_buffering++;
		//start buffering
		gf_clock_buffer_on(odm->ck);
		odm->has_seen_eos = GF_FALSE;

		if ((odm->type!=GF_STREAM_VISUAL) && (odm->type!=GF_STREAM_AUDIO)) {
			if (gf_list_find(scene->compositor->systems_pids, odm->pid)<0)
				gf_list_add(scene->compositor->systems_pids, odm->pid);
		}

		gf_filter_pid_send_event(odm->pid, &com);
	}
	if (odm->extra_pids) {
		u32 k, count = gf_list_count(odm->extra_pids);
		for (k=0; k<count; k++) {
			GF_ODMExtraPid *xpid = gf_list_get(odm->extra_pids, k);
			if (xpid->state != GF_ODM_STATE_PLAY) {
				xpid->state = GF_ODM_STATE_PLAY;
				com.base.on_pid = xpid->pid;
				odm->has_seen_eos = GF_FALSE;
				odm->nb_buffering ++;
				scene->nb_buffering++;
				//start buffering
				gf_clock_buffer_on(odm->ck);

				if ((odm->type!=GF_STREAM_VISUAL) && (odm->type!=GF_STREAM_AUDIO)) {
					if (gf_list_find(scene->compositor->systems_pids, xpid->pid)<0)
						gf_list_add(scene->compositor->systems_pids, xpid->pid);
				}
				
				gf_filter_pid_send_event(xpid->pid, &com);
			}
		}
	}

	if (odm->parentscene) {
		if (odm->parentscene->root_od->addon) {
			odm->parentscene->root_od->addon->started = 1;
		}
		if (odm->parentscene->first_frame_pause_type) {
			media_control_paused = GF_TRUE;
		}
	} else if (odm->subscene && odm->subscene->first_frame_pause_type) {
		media_control_paused = GF_TRUE;
	}

	gf_odm_service_media_event(odm, GF_EVENT_MEDIA_LOAD_START);
	gf_odm_service_media_event(odm, GF_EVENT_MEDIA_TIME_UPDATE);

	odm->disable_buffer_at_next_play = GF_FALSE;

	odm->disable_buffer_at_next_play = GF_FALSE;

	if (odm->flags & GF_ODM_PAUSE_QUEUED) {
		odm->flags &= ~GF_ODM_PAUSE_QUEUED;
		media_control_paused = 1;
	}

	if (media_control_paused) {
		gf_odm_pause(odm);
	}
}

void gf_odm_stop(GF_ObjectManager *odm, Bool force_close)
{
	u32 i;
	GF_ODMExtraPid *xpid;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
	MediaSensorStack *media_sens;
#endif
	GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
	GF_FilterEvent com;

	odm->flags &= ~GF_ODM_PREFETCH;
	
	//root ODs of dynamic scene may not have seen play/pause request
	if (!odm->state && !odm->scalable_addon && (!odm->subscene || !odm->subscene->is_dynamic_scene) ) return;

	//PID not yet attached, mark as state_stop and wait for error or OK
	if (!odm->pid ) {
		odm->state = GF_ODM_STATE_STOP;
		return;
	}


	if (force_close && odm->mo) odm->mo->flags |= GF_MO_DISPLAY_REMOVE;
	/*stop codecs*/
	if (odm->subscene) {
		u32 i=0;
		GF_ObjectManager *sub_odm;

		/*stops all resources of the subscene as well*/
		while ((sub_odm=(GF_ObjectManager *)gf_list_enum(odm->subscene->resources, &i))) {
			gf_odm_stop(sub_odm, force_close);
		}
	}

	/*send stop command*/
	odm->has_seen_eos = GF_FALSE;
	odm->state = GF_ODM_STATE_STOP;
	GF_FEVT_INIT(com, GF_FEVT_STOP, odm->pid)
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] PID %s At OTB %u requesting STOP\n", odm->ID, odm->scene_ns->url, gf_filter_pid_get_name(odm->pid), gf_clock_time(odm->ck) ));

	gf_filter_pid_send_event(odm->pid, &com);
	gf_list_del_item(scene->compositor->systems_pids, odm->pid);
	i=0;
	while ((xpid = (GF_ODMExtraPid*)gf_list_enum(odm->extra_pids, &i)) ) {
		xpid->has_seen_eos = GF_FALSE;
		xpid->state = GF_ODM_STATE_STOP;
		com.base.on_pid = xpid->pid;
		gf_list_del_item(scene->compositor->systems_pids, xpid->pid);
		gf_filter_pid_send_event(xpid->pid, &com);

	}

	if (odm->parentscene && odm->parentscene->root_od->addon) {
		odm->parentscene->root_od->addon->started = 0;
	}
	if (odm->nb_buffering) {
		assert(scene->nb_buffering>=odm->nb_buffering);
		scene->nb_buffering -= odm->nb_buffering;
		while (odm->nb_buffering) {
			gf_clock_buffer_off(odm->ck);
			odm->nb_buffering --;
		}
		if (!scene->nb_buffering) {
			gf_scene_buffering_info(scene);
		}
	}

	gf_odm_service_media_event(odm, GF_EVENT_ABORT);

	/*stops clock if this is a scene stop*/
	if (!(odm->flags & GF_ODM_INHERIT_TIMELINE) && odm->subscene && odm->owns_clock ) {
		gf_clock_reset(odm->ck);
	}
	odm->media_current_time = 0;

#ifndef GPAC_DISABLE_VRML
	/*reset media sensor(s)*/
	i = 0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i))) {
		MS_Stop(media_sens);
	}
	/*reset media control state*/
	ctrl = gf_odm_get_mediacontrol(odm);
	if (ctrl) ctrl->current_seg = 0;
#endif

}

void gf_odm_on_eos(GF_ObjectManager *odm, GF_FilterPid *pid)
{
	u32 i, count, nb_eos, nb_share_clock, nb_ck_running;
	Bool all_done = GF_TRUE;
#ifndef GPAC_DISABLE_VRML
	if (gf_odm_check_segment_switch(odm)) return;
#endif

	nb_share_clock=0;
	nb_eos = odm->has_seen_eos ? 1 : 0;
	nb_ck_running = 0;

	if (odm->pid==pid) {
		odm->has_seen_eos = GF_TRUE;
	}
	if (!odm->has_seen_eos) all_done = GF_FALSE;

	count = gf_list_count(odm->extra_pids);
	for (i=0; i<count; i++) {
		GF_ODMExtraPid *xpid = gf_list_get(odm->extra_pids, i);
		if (xpid->pid == pid) {
			xpid->has_seen_eos = GF_TRUE;
		}
		if (!xpid->has_seen_eos) all_done = GF_FALSE;
	}
	if (!all_done) return;

	if (odm->addon && odm->addon->is_splicing)
		odm->addon->is_over = 1;
	if (odm->parentscene && odm->parentscene->root_od->addon && odm->parentscene->root_od->addon->is_splicing)
		odm->parentscene->root_od->addon->is_over = 1;

	if (odm->ck->has_seen_eos) return;

	odm->ck->has_seen_eos = 1;
	gf_odm_check_buffering(odm, pid);

#ifndef GPAC_DISABLE_VRML
	//check for scene restart upon end of stream
	if (odm->subscene) {
		gf_scene_mpeg4_inline_check_restart(odm->subscene);
	}
#endif

	gf_odm_service_media_event(odm, GF_EVENT_MEDIA_LOAD_DONE);
	//a little optimization here: for scene with no associated resources (no audio, video, images), unload
	//the filter chain once the scene is loaded
	//TODO: further optimize to disconnect scenes with static resources (images, logo, ...)
	if (odm->subscene && !gf_list_count(odm->subscene->resources)) {
		GF_FilterEvent fevt;

		GF_FEVT_INIT(fevt, GF_FEVT_RESET_SCENE, odm->pid);
		fevt.attach_scene.object_manager = odm;
		gf_filter_pid_exec_event(odm->pid, &fevt);

		gf_filter_pid_set_udta(odm->pid, NULL);
		odm->pid = NULL;
		for (i=0; i<count; i++) {
			GF_ODMExtraPid *xpid = gf_list_get(odm->extra_pids, i);
			gf_filter_pid_set_udta(xpid->pid, NULL);
			xpid->pid = NULL;
		}
		gf_filter_remove(odm->scene_ns->source_filter, odm->subscene->compositor->filter);
		odm->scene_ns->source_filter = NULL;
	}
}

void gf_odm_signal_eos_reached(GF_ObjectManager *odm)
{
	if (odm->parentscene && !gf_scene_is_root(odm->parentscene) ) {
		GF_ObjectManager *root = odm->parentscene->root_od;
		Bool is_over = 0;

		if (!gf_scene_check_clocks(root->scene_ns, root->subscene, 0)) return;
		if (root->subscene->is_dynamic_scene)
			is_over = 1;
		else
			is_over = gf_sc_is_over(odm->parentscene->compositor, root->subscene->graph);

		if (is_over) {
			gf_odm_service_media_event(root, GF_EVENT_MEDIA_ENDED);
		}
	} else {
		GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
		if (gf_sc_check_end_of_scene(scene->compositor, 0)) {
			GF_Event evt;
			evt.type = GF_EVENT_EOS;
			gf_sc_send_event(odm->parentscene->compositor, &evt);
		}
	}
}


void gf_odm_set_timeshift_depth(GF_ObjectManager *odm, u32 stream_timeshift)
{
	GF_Scene *scene;
	if (odm->timeshift_depth != stream_timeshift)
		odm->timeshift_depth = stream_timeshift;

	scene = odm->subscene ? odm->subscene : odm->parentscene;

	/*update scene duration*/
	gf_scene_set_timeshift_depth(scene);
}


GF_Clock *gf_odm_get_media_clock(GF_ObjectManager *odm)
{
	while (odm->lower_layer_odm) {
		odm = odm->lower_layer_odm;
	}
	if (odm->ck) return odm->ck;
	return NULL;
}


Bool gf_odm_shares_clock(GF_ObjectManager *odm, GF_Clock *ck)
{
	if (odm->ck == ck) return GF_TRUE;
	return GF_FALSE;
}


void gf_odm_pause(GF_ObjectManager *odm)
{
	u32 i;
#ifndef GPAC_DISABLE_VRML
	MediaSensorStack *media_sens;
#endif
	GF_ODMExtraPid *xpid;
	GF_FilterEvent com;
	GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
	//postpone until the PLAY request
	if (odm->state != GF_ODM_STATE_PLAY) {
		odm->flags |= GF_ODM_PAUSE_QUEUED;
		return;
	}

	if (odm->flags & GF_ODM_PAUSED) return;
	odm->flags |= GF_ODM_PAUSED;

	//cleanup - we need to enter in stop state for broadcast modes
	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

	scene = gf_scene_get_root_scene(scene);

	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] PID %s: At OTB %u requesting PAUSE (clock init %d)\n", odm->ID, odm->scene_ns->url, gf_filter_pid_get_name(odm->pid), gf_clock_time(odm->ck), odm->ck->clock_init ));

	GF_FEVT_INIT(com, GF_FEVT_PAUSE, odm->pid);
	gf_clock_pause(odm->ck);

	if ((odm->state == GF_ODM_STATE_PLAY) && (scene->first_frame_pause_type!=2)) {
		gf_filter_pid_send_event(odm->pid, &com);
	}

	i=0;
	while ((xpid = (GF_ODMExtraPid*)gf_list_enum(odm->extra_pids, &i))) {
		gf_clock_pause(odm->ck);
		if (xpid->state != GF_ODM_STATE_PLAY) continue;

		//if we are in dump mode, the clocks are paused (step-by-step render), but we don't send the pause commands to
		//the network !
		if (scene->first_frame_pause_type!=2) {
			com.base.on_pid = xpid->pid;
			gf_filter_pid_send_event(xpid->pid, &com);
		}
	}

	//if we are in dump mode, only the clocks are paused (step-by-step render), the media object is still in play state
	if (scene->first_frame_pause_type==2) {
		return;
	}

#ifndef GPAC_DISABLE_VRML
	/*mediaSensor  shall generate isActive false when paused*/
	i=0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i)) ) {
		if (media_sens && media_sens->sensor->isActive) {
			media_sens->sensor->isActive = 0;
			gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
		}
	}
#endif

}

void gf_odm_resume(GF_ObjectManager *odm)
{
	u32 i;

#ifndef GPAC_DISABLE_VRML
	MediaSensorStack *media_sens;
	MediaControlStack *ctrl;
#endif

	GF_ODMExtraPid *xpid;
	GF_FilterEvent com;

	if (odm->flags & GF_ODM_PAUSE_QUEUED) {
		odm->flags &= ~GF_ODM_PAUSE_QUEUED;
		return;
	}

	if (!(odm->flags & GF_ODM_PAUSED)) return;
	odm->flags &= ~GF_ODM_PAUSED;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

#ifndef GPAC_DISABLE_VRML
	ctrl = gf_odm_get_mediacontrol(odm);
#endif

	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] CH%d: At OTB %u requesting RESUME (clock init %d)\n", odm->ID, odm->scene_ns->url, gf_filter_pid_get_name(odm->pid), gf_clock_time(odm->ck), odm->ck->clock_init ));

	GF_FEVT_INIT(com, GF_FEVT_RESUME, odm->pid);

	gf_clock_resume(odm->ck);
	if (odm->state == GF_ODM_STATE_PLAY)
		gf_filter_pid_send_event(odm->pid, &com);

	i=0;
	while ((xpid = (GF_ODMExtraPid*)gf_list_enum(odm->extra_pids, &i)) ) {
		gf_clock_resume(odm->ck);

		if (odm->state != GF_ODM_STATE_PLAY) continue;

		com.base.on_pid = xpid->pid;
		gf_filter_pid_send_event(odm->pid, &com);
	}

#ifndef GPAC_DISABLE_VRML
	/*override speed with MC*/
	if (ctrl) {
		gf_clock_set_speed(odm->ck, ctrl->control->mediaSpeed);
	}

	/*mediaSensor shall generate isActive TRUE when resumed*/
	i=0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i)) ) {
		if (media_sens && !media_sens->sensor->isActive) {
			media_sens->sensor->isActive = 1;
			gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
		}
	}
#endif
}

void gf_odm_set_speed(GF_ObjectManager *odm, Fixed speed, Bool adjust_clock_speed)
{
	u32 i;
	GF_ODMExtraPid *xpid;
	GF_FilterEvent com;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;
	if (!odm->pid) return;

	if (adjust_clock_speed)
		gf_clock_set_speed(odm->ck, speed);

	GF_FEVT_INIT(com, GF_FEVT_SET_SPEED, odm->pid);

	com.play.speed = FIX2FLT(speed);
	gf_filter_pid_send_event(odm->pid, &com);

	i=0;
	while ((xpid = (GF_ODMExtraPid*)gf_list_enum(odm->extra_pids, &i)) ) {

		com.play.on_pid = xpid->pid;
		gf_filter_pid_send_event(xpid->pid, &com);
	}
}

GF_Segment *gf_odm_find_segment(GF_ObjectManager *odm, char *descName)
{
#if FILTER_FIXME
	GF_Segment *desc;
	u32 i = 0;
	if (!odm->OD) return NULL;
	while ( (desc = (GF_Segment *)gf_list_enum(odm->OD->OCIDescriptors, &i)) ) {
		if (desc->tag != GF_ODF_SEGMENT_TAG) continue;
		if (!stricmp(desc->SegmentName, descName)) return desc;
	}
#endif
	return NULL;
}

#if FILTER_FIXME
static void gf_odm_insert_segment(GF_ObjectManager *odm, GF_Segment *seg, GF_List *list)
{
	/*this reorders segments when inserting into list - I believe this is not compliant*/
#if 0
	GF_Segment *desc;
	u32 i = 0;
	while ((desc = gf_list_enum(list, &i))) {
		if (desc == seg) return;
		if (seg->startTime + seg->Duration <= desc->startTime) {
			gf_list_insert(list, seg, i);
			return;
		}
	}
#endif
	gf_list_add(list, seg);
}
#endif

/*add segment descriptor and sort them*/
void gf_odm_init_segments(GF_ObjectManager *odm, GF_List *list, MFURL *url)
{
#if FILTER_FIXME
	char *str, *sep;
	char seg1[1024], seg2[1024], seg_url[4096];
	GF_Segment *first_seg, *last_seg, *seg;
	u32 i, j;

	/*browse all URLs*/
	for (i=0; i<url->count; i++) {
		if (!url->vals[i].url) continue;
		str = strstr(url->vals[i].url, "#");
		if (!str) continue;
		str++;
		strcpy(seg_url, str);
		/*segment closed range*/
		if ((sep = strstr(seg_url, "-")) ) {
			strcpy(seg2, sep+1);
			sep[0] = 0;
			strcpy(seg1, seg_url);
			first_seg = gf_odm_find_segment(odm, seg1);
			if (!first_seg) continue;
			last_seg = gf_odm_find_segment(odm, seg2);
		}
		/*segment open range*/
		else if ((sep = strstr(seg_url, "+")) ) {
			sep[0] = 0;
			strcpy(seg1, seg_url);
			first_seg = gf_odm_find_segment(odm, seg_url);
			if (!first_seg) continue;
			last_seg = NULL;
		}
		/*single segment*/
		else {
			first_seg = gf_odm_find_segment(odm, seg_url);
			if (!first_seg) continue;
			gf_odm_insert_segment(odm, first_seg, list);
			continue;
		}
		/*segment range process*/
		gf_odm_insert_segment(odm, first_seg, list);
		j=0;
		while ( (seg = (GF_Segment *)gf_list_enum(odm->OD->OCIDescriptors, &j)) ) {
			if (seg->tag != GF_ODF_SEGMENT_TAG) continue;
			if (seg==first_seg) continue;
			if (seg->startTime + seg->Duration <= first_seg->startTime) continue;
			/*this also includes last_seg insertion !!*/
			if (last_seg && (seg->startTime + seg->Duration > last_seg->startTime + last_seg->Duration) ) continue;
			gf_odm_insert_segment(odm, seg, list);
		}
	}
#endif
}

Bool gf_odm_check_buffering(GF_ObjectManager *odm, GF_FilterPid *pid)
{
	u32 timescale;
	Bool signal_eob = GF_FALSE;
	GF_Scene *scene;
	GF_FilterClockType ck_type;
	u64 clock_reference;
	GF_FilterPacket *pck;

	assert(odm);

	if (!pid)
		pid = odm->pid;

	scene = odm->subscene ? odm->subscene : odm->parentscene;

	pck = gf_filter_pid_get_packet(pid);
	ck_type = gf_filter_pid_get_clock_info(pid, &clock_reference, &timescale);

	if (!odm->ck->clock_init && ck_type) {
		clock_reference *= 1000;
		clock_reference /= timescale;
		gf_clock_set_time(odm->ck, clock_reference);
		if (odm->parentscene)
			odm->parentscene->root_od->media_start_time = 0;
	}

	if (odm->nb_buffering) {
		u64 buffer_duration = gf_filter_pid_query_buffer_duration(pid);
		if ( ! odm->ck->clock_init) {
			u64 time;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
			if (!pck) return GF_TRUE;
			timescale = gf_filter_pck_get_timescale(pck);

			time = gf_filter_pck_get_cts(pck);
			if (time==GF_FILTER_NO_TS) time = gf_filter_pck_get_dts(pck);
			time *= 1000;
			time /= timescale;
			gf_clock_set_time(odm->ck, time);
			if (odm->parentscene)
				odm->parentscene->root_od->media_start_time = 0;
		}
		//TODO abort buffering when errors are found on the input chain !!
		if (buffer_duration >= odm->buffer_playout_us) {
			odm->nb_buffering --;
			scene->nb_buffering--;
			if (!scene->nb_buffering) {
				signal_eob = GF_TRUE;
			}
			gf_clock_buffer_off(odm->ck);
		} else if (gf_filter_pid_has_seen_eos(pid) ) {
			odm->nb_buffering --;
			scene->nb_buffering--;
			if (!scene->nb_buffering) {
				signal_eob = GF_TRUE;
			}
			gf_clock_buffer_off(odm->ck);
		}

	}
	if (scene->nb_buffering || signal_eob)
		gf_scene_buffering_info(scene);

	//handle both PCR discontinuities or TS looping when no PCR disc is present/signaled
	if (pck) {
		s32 diff=0;
		u64 pck_time = 0;
		u32 clock_time = gf_clock_time(odm->ck);
		s32 diff_to = 0;
		if (ck_type) {
			clock_reference *= 1000;
			clock_reference /= timescale;
			diff = (s32) clock_time + odm->buffer_playout_us/1000;
			diff -= (s32) clock_reference;
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("Clock %d (ODM %d) reference found "LLU" ms clock time %d ms - diff %d - type %d\n", odm->ck->clock_id, odm->ID, clock_reference, clock_time, diff, ck_type));

			//if explicit clock discontinuity, mark clock
			if (ck_type==GF_FILTER_CLOCK_PCR_DISC)
				odm->ck->ocr_discontinuity_time = 1+clock_reference;
		}
		pck_time = gf_filter_pck_get_cts(pck);
		timescale = gf_filter_pck_get_timescale(pck);
		if (pck_time != GF_FILTER_NO_TS) {
			pck_time *= 1000;
			pck_time /= timescale;
			pck_time += 1;
			diff = clock_time;
			diff -= pck_time;
			diff_to = odm->ck->ocr_discontinuity_time ? 500 : 8000;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("Clock %d (ODM %d) pck time %d - clock ref "LLU" clock time %d - diff %d vs %d\n", odm->ck->clock_id, odm->ID, pck_time, clock_reference, clock_time, diff, diff_to));

		//we have a valid TS for the packet, and the CTS diff to the current clock is larget than 8 sec, check for discontinuities
		//it may happen that video is sent up to 4 or 5 seconds ahead of the PCR in some systems, 8 sec should be enough
		if (diff_to && (ABS(diff) > diff_to) ) {
			s64 diff_pck_old_clock, diff_pck_new_clock;
			//compute diff to old clock and new clock
			diff_pck_new_clock = pck_time-1 - (s64) clock_reference;
			if (diff_pck_new_clock<0) diff_pck_new_clock = -diff_pck_new_clock;
			diff_pck_old_clock = pck_time-1 - (s64) clock_time;
			if (diff_pck_old_clock<0) diff_pck_old_clock = -diff_pck_old_clock;

			//if the packet time is closer to the new clock than the old, switch to new clock
			if (diff_pck_old_clock > diff_pck_new_clock) {
				u32 i, count;
				GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
				GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("Clock %d (ODM %d) discontinuity detected "LLU" clock time %d - diff %d - type %d - pck time "LLU"\n", odm->ck->clock_id, odm->ID, clock_reference, clock_time, diff, ck_type, pck_time-1));

				count = gf_list_count(scene->resources);
				for (i=0; i<count; i++) {
					GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);
					if (an_odm->ck != odm->ck) continue;
					an_odm->prev_clock_at_discontinuity_plus_one = 1 + clock_time;
				}
				odm->ck->clock_init = GF_FALSE;
				gf_clock_set_time(odm->ck, odm->ck->ocr_discontinuity_time ? odm->ck->ocr_discontinuity_time - 1 : clock_reference);
				odm->ck->ocr_discontinuity_time = 0;
			}
		}
	} else if (ck_type) {
		clock_reference *= 1000;
		clock_reference /= timescale;
		if (ck_type==GF_FILTER_CLOCK_PCR_DISC)
			odm->ck->ocr_discontinuity_time = 1 + clock_reference;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("Clock %d (ODM %d) received "LLU" type %d clock time %d no pending packets\n", odm->ck->clock_id, odm->ID, clock_reference, ck_type, gf_clock_time(odm->ck)));
	}
	return odm->ck->nb_buffering ? GF_TRUE : GF_FALSE;
}

#ifndef GPAC_DISABLE_SVG
void gf_odm_collect_buffer_info(GF_SceneNamespace *scene_ns, GF_ObjectManager *odm, GF_DOMMediaEvent *media_event, u32 *min_time, u32 *min_buffer)
{
	GF_ODMExtraPid *xpid;
	u32 i, val;
	u64 buf_val;

	*min_time = 0;
	*min_buffer = 0;

	if (!odm->pid) return;
	if (odm->scene_ns != scene_ns) return;
	if (! odm->buffer_playout_us) {
		media_event->bufferValid = GF_FALSE;
		return;
	}

	if (odm->nb_buffering)
		media_event->bufferValid = GF_TRUE;

	buf_val = gf_filter_pid_query_buffer_duration(odm->pid);
	if (buf_val > odm->buffer_playout_us) buf_val = odm->buffer_playout_us;
	val = (buf_val * 100) / odm->buffer_playout_us;
	if (*min_buffer > val) *min_buffer = val;

	if (*min_time > (u32) buf_val / 1000)
		*min_time = (u32) buf_val / 1000;

	i=0;
	while ((xpid = gf_list_enum(odm->extra_pids, &i))) {

		buf_val = gf_filter_pid_query_buffer_duration(odm->pid);
		if (buf_val > odm->buffer_playout_us) buf_val = odm->buffer_playout_us;
		val = (buf_val * 100) / odm->buffer_playout_us;
		if (*min_buffer > val) *min_buffer = val;

		if (*min_time > (u32) buf_val / 1000)
			*min_time = (u32) buf_val / 1000;
	}
}
#endif

void gf_odm_service_media_event_with_download(GF_ObjectManager *odm, GF_EventType event_type, u64 loaded_size, u64 total_size, u32 bytes_per_sec)
{
#ifndef GPAC_DISABLE_SVG
	u32 i, count, min_buffer, min_time;
	GF_DOM_Event evt;
	GF_ObjectManager *an_od;
	GF_Scene *scene;

	if (!odm || !odm->scene_ns) return;
	if (odm->mo) {
		count = gf_mo_event_target_count(odm->mo);

		//for dynamic scenes, check if we have listeners on the root object of the scene containing this media
		if (odm->parentscene
		        && odm->parentscene->is_dynamic_scene
		        && odm->parentscene->root_od->mo
		        && (odm->parentscene->root_od->scene_ns==odm->scene_ns)
		   ) {
			odm = odm->parentscene->root_od;
			count = gf_mo_event_target_count(odm->mo);
		}
		if (!count) return;
	} else {
		count = 0;
	}


	memset(&evt, 0, sizeof(GF_DOM_Event));

	evt.media_event.bufferValid = GF_FALSE;
	evt.media_event.session_name = odm->scene_ns->url;

	min_time = min_buffer = (u32) -1;
	scene = odm->subscene ? odm->subscene : odm->parentscene;
	if (!scene) return;
	
	/*get buffering on root OD*/
	gf_odm_collect_buffer_info(odm->scene_ns, scene->root_od, &evt.media_event, &min_time, &min_buffer);

	/*get buffering on all ODs*/
	i=0;
	while ((an_od = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (odm->scene_ns == an_od->scene_ns)
			gf_odm_collect_buffer_info(odm->scene_ns, an_od, &evt.media_event, &min_time, &min_buffer);
	}

	if (min_buffer != (u32) -1)
		evt.media_event.level = min_buffer;
	if (min_time != (u32) -1)
		evt.media_event.remaining_time = INT2FIX(min_time) / 60;
	evt.media_event.status = 0;
	evt.media_event.loaded_size = loaded_size;
	evt.media_event.total_size = total_size;

	evt.type = event_type;
	evt.bubbles = 0;	/*the spec says yes but we force it to NO*/

	//these events may be triggered from any input or decoding threads. Sync processing cannot be
	//achieved in most cases, because we may run into deadlocks, especially if the event
	//was triggered by a service opened by JS
	for (i=0; i<count; i++) {
		GF_DOMEventTarget *target = (GF_DOMEventTarget *)gf_list_get(odm->mo->evt_targets, i);
		if (target)
			gf_sc_queue_dom_event_on_target(scene->compositor, &evt, target, scene->graph);
	}
	if (!count) {
		GF_Node *root = gf_sg_get_root_node(scene->graph);
		if (root) gf_sc_queue_dom_event(scene->compositor, root, &evt);
	}
#endif
}

void gf_odm_service_media_event(GF_ObjectManager *odm, GF_EventType event_type)
{
	gf_odm_service_media_event_with_download(odm, event_type, 0, 0, 0);
}

void gf_odm_stop_or_destroy(GF_ObjectManager *odm)
{
	Bool destroy = GF_FALSE;
	if (odm->mo ) {
		if (odm->addon) odm->flags |= GF_ODM_REGENERATE_SCENE;
		else if (odm->mo->OD_ID==GF_MEDIA_EXTERNAL_ID) destroy = GF_TRUE;
		else if (odm->ID==GF_MEDIA_EXTERNAL_ID) destroy = GF_TRUE;
	}
	if (destroy) {
		gf_odm_disconnect(odm, 2);
	} else {
		gf_odm_stop(odm, 0);
	}
}


static void get_codec_stats(GF_FilterPid *pid, GF_MediaInfo *info)
{
	GF_FilterPidStatistics stats;
	gf_filter_pid_get_statistics(pid, &stats);

	info->avg_bitrate = stats.avgerage_bitrate;
	info->max_bitrate = stats.max_bitrate;
	info->nb_dec_frames = stats.nb_processed;
	info->max_dec_time = stats.max_process_time;
	info->total_dec_time = stats.total_process_time;
	info->first_frame_time = stats.first_process_time;
	info->last_frame_time = stats.last_process_time;
	info->au_duration = stats.min_frame_dur;
	info->nb_iraps = stats.nb_saps;
	info->irap_max_dec_time = stats.max_sap_process_time;
	info->irap_total_dec_time = stats.total_sap_process_time;
	info->avg_process_bitrate = stats.average_process_rate;
	info->max_process_bitrate = stats.max_process_rate;
}

GF_EXPORT
GF_Err gf_odm_get_object_info(GF_ObjectManager *odm, GF_MediaInfo *info)
{
	const GF_PropertyValue *prop;
	GF_ObjectManager *an_odm;
	GF_FilterPid *pid;

	if (!odm || !info) return GF_BAD_PARAM;
	memset(info, 0, sizeof(GF_MediaInfo));

	info->ODID = odm->ID;
	info->ServiceID = odm->ServiceID;
	info->pid_id = odm->pid_id;
	info->ocr_id = odm->ck ? odm->ck->clock_id : 0;
	info->od_type = odm->type;

	info->duration = (Double) (s64)odm->duration;
	info->duration /= 1000;

	pid = odm->pid;
	an_odm = odm;
	while (an_odm->lower_layer_odm) {
		an_odm = an_odm->lower_layer_odm;
		pid = an_odm->pid;
	}

	if (pid) {
		/*since we don't remove ODs that failed setup, check for clock*/
		if (odm->ck) {
			info->current_time = odm->media_current_time;
			info->ntp_diff = odm->last_drawn_frame_ntp_diff;
			info->current_time = gf_clock_media_time(odm->ck);

		}
		info->current_time /= 1000;
		info->nb_dropped = odm->nb_dropped;
	} else if (odm->subscene) {
		if (odm->subscene->root_od->ck) {
			info->current_time = gf_clock_media_time(odm->subscene->root_od->ck);
			info->current_time /= 1000;
		}
		info->duration = (Double) (s64)odm->subscene->duration;
		info->duration /= 1000;
		info->nb_dropped = odm->subscene->root_od->nb_dropped;
		info->generated_scene = odm->subscene->is_dynamic_scene;
	}
	if (info->duration && info->current_time>info->duration)
		info->current_time = info->duration;

	info->buffer = -2;
	info->db_unit_count = 0;

	if (odm->state) {
		GF_Clock *ck;

		ck = gf_odm_get_media_clock(odm);
		/*no clock means setup failed*/
		if (!ck) {
			info->status = 4;
		} else {
			info->status = gf_clock_is_started(ck) ? 1 : 2;
			info->clock_drift = ck->drift;

			info->buffer = -1;
			info->min_buffer = -1;
			info->max_buffer = 0;

			if (pid)
				info->buffer = gf_filter_pid_query_buffer_duration(pid) / 1000;
			info->max_buffer = odm->buffer_max_us / 1000;
			info->min_buffer = odm->buffer_min_us / 1000;

#ifdef FILTER_FIXME
			info->protection = ch->ipmp_tool ? 1 : 2;
#endif
		}
	}

	if (odm->scene_ns) {
		info->service_handler = odm->scene_ns->source_filter ? gf_filter_get_name(odm->scene_ns->source_filter) : "unloaded";

		info->service_url = odm->scene_ns->url;
		if (odm->scene_ns->owner == odm) info->owns_service = 1;
	} else if ((odm->subscene && odm->subscene->graph_attached) || (odm->ID)) {
		info->service_url = "No associated network Service";
	} else {
		info->service_url = "Service not found or error";
	}

	if (pid) {
		info->codec_name = gf_filter_pid_get_filter_name(pid);
		info->od_type = odm->type;

		gf_filter_pid_get_buffer_occupancy(pid, &info->cb_max_count, &info->cb_unit_count, NULL, NULL);

		get_codec_stats(pid, info);

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
		if (prop) info->lang_code = prop->value.string;
	}

	if (odm->subscene) {
		gf_sg_get_scene_size_info(odm->subscene->graph, &info->width, &info->height);
	} else if (odm->mo) {
		switch (info->od_type) {
		case GF_STREAM_VISUAL:
			gf_mo_get_visual_info(odm->mo, &info->width, &info->height, NULL, &info->par, &info->pixelFormat, NULL);
			break;
		case GF_STREAM_AUDIO:
			gf_mo_get_audio_info(odm->mo, &info->sample_rate, &info->bits_per_sample, &info->num_channels, NULL);
			info->clock_drift = 0;
			break;
		case GF_STREAM_TEXT:
			gf_mo_get_visual_info(odm->mo, &info->width, &info->height, NULL, NULL, NULL, NULL);
			break;
		}
	}

	if (odm->mo && odm->mo->URLs.count)
		info->media_url = odm->mo->URLs.vals[0].url;
	return GF_OK;
}


