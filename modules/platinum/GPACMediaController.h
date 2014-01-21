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

#ifndef _GPAC_MEDIA_CONTROLLER_H_
#define _GPAC_MEDIA_CONTROLLER_H_

#include "PltMediaServer.h"
#include "PltMediaController.h"
#include "PltMediaBrowser.h"
#include "NptMap.h"
#include "NptStack.h"

#include <gpac/thread.h>

typedef struct {
    NPT_SharedVariable shared_var;
    NPT_Result         res;
    PLT_BrowseInfo     info;
} GPAC_BrowseData;

/*basic class to hold each media renderer*/
class GPAC_MediaRendererItem
{
public:
	GPAC_MediaRendererItem(PLT_DeviceDataReference device, NPT_String uuid) : m_device(device), m_UUID(uuid) { }
	PLT_DeviceDataReference m_device;
	NPT_String m_UUID;
};

/*basic class to hold each media server*/
class GPAC_MediaServerItem
{
public:
	GPAC_MediaServerItem(PLT_DeviceDataReference device, NPT_String uuid) : m_device(device), m_UUID(uuid) , m_BrowseResults(NULL) { }
	PLT_DeviceDataReference m_device;
	NPT_String m_UUID;
	/*results of the last browse request on this server*/
	PLT_MediaObjectListReference m_BrowseResults;
	/*parent directory stack*/
	NPT_Stack<NPT_String> m_ParentDirectories;
};

typedef NPT_Reference<GPAC_BrowseData> GPAC_BrowseDataReference;

class GF_UPnP;

class GPAC_MediaController : public PLT_CtrlPointListener, 
							public PLT_MediaControllerDelegate,
							public PLT_MediaBrowserDelegate
{
public:
    GPAC_MediaController(PLT_CtrlPointReference& ctrlPoint, GF_UPnP *upnp);
    ~GPAC_MediaController();

	virtual NPT_Result OnDeviceAdded(PLT_DeviceDataReference& device) 
	{
		return NPT_SUCCESS;
	}
    virtual NPT_Result OnDeviceRemoved(PLT_DeviceDataReference& device)
	{
		return NPT_SUCCESS;
	}
	virtual NPT_Result OnActionResponse(NPT_Result           res, 
                                      PLT_ActionReference& action, 
                                      void*                userdata);
    virtual NPT_Result OnEventNotify(PLT_Service* service, NPT_List<PLT_StateVariable*>* vars);

    // PLT_MediaControllerDelegate
	virtual bool OnMRAdded(PLT_DeviceDataReference& device);
	virtual void OnMRRemoved(PLT_DeviceDataReference& device);
    virtual void OnMRStateVariablesChanged(PLT_Service* /* service */, NPT_List<PLT_StateVariable*>* /* vars */);

	//PLT_MediaBrowserDelegate
    virtual bool OnMSAdded(PLT_DeviceDataReference& device);
    virtual void OnMSRemoved(PLT_DeviceDataReference& device);
    virtual void OnMSStateVariablesChanged(PLT_Service *service, NPT_List<PLT_StateVariable*>* vars);
	virtual void OnBrowseResult(NPT_Result               res, 
                                  PLT_DeviceDataReference& device, 
                                  PLT_BrowseInfo*          info, 
                                  void*                    userdata);

	NPT_Result Browse(GPAC_MediaServerItem *server, const char *id, const char *filter);


	GF_List *m_MediaRenderers;
	GF_List *m_MediaServers;

    /* The UPnP MediaRenderer control point. */
    PLT_MediaController*    m_MediaController;
    /* The UPnP MediaServer control point. */
    PLT_MediaBrowser *m_MediaBrowser;

protected:
    NPT_Result Browse(GPAC_BrowseDataReference& browse_data,
                      PLT_DeviceDataReference& device, 
                      const char*              object_id,
                      NPT_Int32                index, 
                      NPT_Int32                count,
                      bool                     browse_metadata = false,
                      const char*              filter = "*", 
                      const char*              sort = "");

private:
    NPT_Result  WaitForResponse(NPT_SharedVariable& shared_var);

	GF_UPnP *m_pUPnP;
	GF_Mutex *m_ControlPointLock;

};

#endif /* _GPAC_MEDIA_CONTROLLER_H_ */

