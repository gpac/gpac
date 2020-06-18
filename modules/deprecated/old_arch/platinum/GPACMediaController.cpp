
/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2009-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / Platinum UPnP module
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
 *
 *	----------------------------------------------------------------------------------
 *		PLATINUM IS LICENSED UNDER GPL or commercial agreement - cf platinum license
 *	----------------------------------------------------------------------------------
 *
 */


#include "GPACPlatinum.h"

GPAC_MediaController::GPAC_MediaController(PLT_CtrlPointReference& ctrlPoint, GF_UPnP *upnp)
{
	m_MediaController = new PLT_MediaController(ctrlPoint, this);
	m_MediaBrowser = new PLT_MediaBrowser(ctrlPoint, this);

	m_MediaServers = gf_list_new();
	m_MediaRenderers = gf_list_new();
	m_ControlPointLock = gf_mx_new("AVControlPoint");
	m_pUPnP = upnp;
}


GPAC_MediaController::~GPAC_MediaController()
{
	delete m_MediaController;
	m_MediaController=NULL;
	delete m_MediaBrowser;
	m_MediaBrowser=NULL;

	while (gf_list_count(m_MediaServers)) {
		GPAC_MediaServerItem*ms = (GPAC_MediaServerItem*)gf_list_get(m_MediaServers, 0);
		gf_list_rem(m_MediaServers, 0);
		delete ms;
	}
	gf_list_del(m_MediaServers);

	while (gf_list_count(m_MediaRenderers)) {
		GPAC_MediaRendererItem *ms = (GPAC_MediaRendererItem *)gf_list_get(m_MediaRenderers, 0);
		gf_list_rem(m_MediaRenderers, 0);
		delete ms;
	}
	gf_list_del(m_MediaRenderers);

	gf_mx_del(m_ControlPointLock);
}




bool
GPAC_MediaController::OnMRAdded(PLT_DeviceDataReference& device)
{
	NPT_String uuid = device->GetUUID();

	gf_mx_p(m_ControlPointLock);

	// test if it's a media renderer
	PLT_Service* service;
	if (NPT_SUCCEEDED(device->FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
		gf_list_add(m_MediaRenderers, new GPAC_MediaRendererItem(device, uuid) );
	}
	m_pUPnP->OnMediaRendererAdd(device, 1);
	gf_mx_v(m_ControlPointLock);
	return true;
}


void
GPAC_MediaController::OnMRRemoved(PLT_DeviceDataReference& device)
{
	NPT_String uuid = device->GetUUID();

	gf_mx_p(m_ControlPointLock);

	u32 i, count;
	count = gf_list_count(m_MediaRenderers);
	for (i=0; i<count; i++) {
		GPAC_MediaRendererItem *ms = (GPAC_MediaRendererItem *) gf_list_get(m_MediaRenderers, i);
		if (ms->m_UUID==uuid) {
			delete ms;
			gf_list_rem(m_MediaRenderers, i);
			break;
		}
	}

	m_pUPnP->OnMediaRendererAdd(device, 0);
	gf_mx_v(m_ControlPointLock);
}

bool
GPAC_MediaController::OnMSAdded(PLT_DeviceDataReference& device)
{
	NPT_String uuid = device->GetUUID();

	gf_mx_p(m_ControlPointLock);
	// test if it's a media server
	PLT_Service* service;
	if (NPT_SUCCEEDED(device->FindServiceByType("urn:schemas-upnp-org:service:ContentDirectory:1", service))) {
		gf_list_add(m_MediaServers, new GPAC_MediaServerItem(device, uuid) );
	}
	m_pUPnP->OnMediaServerAdd(device, 1);
	gf_mx_v(m_ControlPointLock);
	return true;
}


void
GPAC_MediaController::OnMSRemoved(PLT_DeviceDataReference& device)
{
	NPT_String uuid = device->GetUUID();

	gf_mx_p(m_ControlPointLock);
	u32 i, count;
	count = gf_list_count(m_MediaServers);
	for (i=0; i<count; i++) {
		GPAC_MediaServerItem *ms = (GPAC_MediaServerItem *) gf_list_get(m_MediaServers, i);
		if (ms->m_UUID==uuid) {
			delete ms;
			gf_list_rem(m_MediaServers, i);
			break;
		}
	}
	m_pUPnP->OnMediaServerAdd(device, 0);
	gf_mx_v(m_ControlPointLock);
}



NPT_Result
GPAC_MediaController::OnActionResponse(NPT_Result           res,
                                       PLT_ActionReference& action,
                                       void*                userdata)
{
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaController::OnEventNotify(PLT_Service* service, NPT_List<PLT_StateVariable*>* vars)
{
	return NPT_SUCCESS;
}

void GPAC_MediaController::OnMRStateVariablesChanged(PLT_Service* service, NPT_List<PLT_StateVariable*>* vars )
{
	u32 count;
	u32 i;
	s32 render_idx = -1;

	count = gf_list_count(m_MediaRenderers);
	for (i=0; i<count; i++) {
		GPAC_MediaRendererItem *mr = (GPAC_MediaRendererItem *) gf_list_get(m_MediaRenderers, i);
		if ( mr->m_device.AsPointer() == service->GetDevice() ) {
			render_idx = i;
			break;
		}
	}
	if (render_idx < 0) return;

	count = vars->GetItemCount();
	for (i=0; i<count; i++) {
		PLT_StateVariable *svar;
		vars->Get(i, svar);
		if (svar->GetName() == NPT_String("AbsoluteTimePosition")) {
			u32 h, m, s;
			if (sscanf((char *) svar->GetValue(), "%d:%d:%d", &h, &m, &s)==3) {
				Double time = h*3600 + m*60 + s;
				this->m_pUPnP->onTimeChanged(render_idx, time);
			}
		}
		else if (svar->GetName() == NPT_String("CurrentTrackDuration")) {
			u32 h, m, s;
			if (sscanf((char *) svar->GetValue(), "%d:%d:%d", &h, &m, &s)==3) {
				Double time = h*3600 + m*60 + s;
				this->m_pUPnP->onDurationChanged(render_idx, time);
			}
		}

	}
}


void GPAC_MediaController::OnBrowseResult(NPT_Result res, PLT_DeviceDataReference& device, PLT_BrowseInfo* info, void* userdata)
{
	NPT_COMPILER_UNUSED(device);

	NPT_COMPILER_UNUSED(device);

	if (!userdata) return;

	GPAC_BrowseDataReference* data = (GPAC_BrowseDataReference*) userdata;
	(*data)->res = res;
	if (NPT_SUCCEEDED(res) && info) {
		(*data)->info = *info;
	}
	(*data)->shared_var.SetValue(1);
	delete data;
}

void
GPAC_MediaController::OnMSStateVariablesChanged(PLT_Service* service, NPT_List<PLT_StateVariable*>* vars)
{
	GPAC_MediaServerItem *ms = NULL;
	gf_mx_p(m_ControlPointLock);

	u32 i, count;
	count = gf_list_count(m_MediaServers);
	for (i=0; i<count; i++) {
		GPAC_MediaServerItem *ms = (GPAC_MediaServerItem *) gf_list_get(m_MediaServers, i);
		if (ms->m_UUID==service->GetDevice()->GetUUID()) {
			break;
		}
		ms = NULL;
	}

	if (!ms) {
		gf_mx_v(m_ControlPointLock);
		return;
	}

	PLT_StateVariable* var = PLT_StateVariable::Find(*vars, "ContainerUpdateIDs");
	if (var) {
		// variable found, parse value
		NPT_String value = var->GetValue();
		NPT_String item_id, update_id;
		int index;

		while (value.GetLength()) {
			// look for container id
			index = value.Find(',');
			if (index < 0) break;
			item_id = value.Left(index);
			value = value.SubString(index+1);

			// look for update id
			if (value.GetLength()) {
				index = value.Find(',');
				update_id = (index<0)?value:value.Left(index);
				value = (index<0)?"":value.SubString(index+1);

				m_pUPnP->ContainerChanged(ms->m_device, item_id, update_id);
			}
		}
	}
	gf_mx_v(m_ControlPointLock);
}

NPT_Result
GPAC_MediaController::WaitForResponse(NPT_SharedVariable& shared_var)
{
	return shared_var.WaitUntilEquals(1, 30000);
}


NPT_Result
GPAC_MediaController::Browse(GPAC_BrowseDataReference& browse_data,
                             PLT_DeviceDataReference& device,
                             const char*              object_id,
                             NPT_Int32                index,
                             NPT_Int32                count,
                             bool                     browse_metadata,
                             const char*              filter,
                             const char*              sort)
{
	NPT_Result res;

	browse_data->shared_var.SetValue(0);

	// send off the browse packet.  Note that this will
	// not block.  There is a call to WaitForResponse in order
	// to block until the response comes back.
	res = m_MediaBrowser->Browse(device,
	                             (const char*)object_id,
	                             index,
	                             count,
	                             browse_metadata,
	                             filter,
	                             sort,
	                             new GPAC_BrowseDataReference(browse_data));
	NPT_CHECK_SEVERE(res);

	return WaitForResponse(browse_data->shared_var);
}

NPT_Result
GPAC_MediaController::Browse(GPAC_MediaServerItem *server, const char *object_id, const char *filter)
{
	NPT_Result res = NPT_FAILURE;
	NPT_Int32  index = 0;

	// reset output params
	server->m_BrowseResults = NULL;


	do {
		GPAC_BrowseDataReference browse_data(new GPAC_BrowseData());

		// send off the browse packet.  Note that this will
		// not block.  There is a call to WaitForResponse in order
		// to block until the response comes back.
		res = Browse(browse_data,
		             server->m_device,
		             (const char*)object_id,
		             index,
		             1024,
		             false,
		             filter,
		             "");
		NPT_CHECK_LABEL_WARNING(res, done);

		if (NPT_FAILED(browse_data->res)) {
			res = browse_data->res;
			NPT_CHECK_LABEL_WARNING(res, done);
		}

		if (browse_data->info.items->GetItemCount() == 0)
			break;

		if (server->m_BrowseResults.IsNull()) {
			server->m_BrowseResults = browse_data->info.items;
		} else {
			server->m_BrowseResults->Add(*browse_data->info.items);
			// clear the list items so that the data inside is not
			// cleaned up by PLT_MediaItemList dtor since we copied
			// each pointer into the new list.
			browse_data->info.items->Clear();
		}

		// stop now if our list contains exactly what the server said it had
		if (browse_data->info.tm && browse_data->info.tm == server->m_BrowseResults->GetItemCount())
			break;

		// ask for the next chunk of entries
		index = server->m_BrowseResults->GetItemCount();
	} while(1);

done:
	return res;
}

