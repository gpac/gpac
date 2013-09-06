/*
 * (c) 2005, 2008 Nico Pranke <Nico.Pranke@googlemail.com>, Robin Luedtke <RobinLu@gmx.de> 
 *
 * This file is part of avcap.
 *
 * avcap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avcap.  If not, see <http://www.gnu.org/licenses/>.
 */

/* avcap is free for non-commercial use.
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>.
 */


#ifndef DSCONTROLMANAGER_H_
#define DSCONTROLMANAGER_H_

#include <string>
#include <list>

#include "ControlManager.h"
#include "DS_Control.h"
#include "avcap-export.h"

namespace avcap
{
class DS_DeviceDescriptor;
	
	//! DirectShow ControlManager implementation.

	/*! DirectShow doesn't provide any methods to enumerate driver- 
	* specific controls like V4L2 does. 
	* There are only few DirectShow COM-Interfaces (e.g. IAMExtTransport (for 
	* Firewire or other external devices), IAMVideoControl, IAMVideoCompression, IAMVideoProcAmp, 
	* IAMCameraControl --> see DirectShow documentation), the capture device DirectShow filter
	* (only WDM capture devices) can expose to support the setting of global properties. The most 
	* important settings are covered by these DirectShow COM-Interfaces. 
	* In the future, DirectShow will provide more COM-Interfaces.
	* 
	* The DirectShow COM-Interfaces IAMVideoProcAmp and IAMCameraControl have been already 
	* implemented in the avcap-library.
	* 
	* Most WDM capture devices and \b all VFW capture devices have their own driver-supplied dialog 
	* boxes with user-setable controls on it, that can't be set programmatically. */
	
	class AVCAP_Export DS_ControlManager : public ControlManager
	{
	private:
		DS_DeviceDescriptor* mDSDeviceDescriptor;
	
	public:
		DS_ControlManager(DS_DeviceDescriptor *dd);
	
		virtual ~DS_ControlManager();
	
		Control* getControl(const std::string& name);
	
		Control* getControl(int id);
	
		const ListType& getControlList()
			{ return (const ListType&) mControls; }
		
		void query();
	
	private:
		void query(int start_id, int end_id);
	};
}

#endif 
