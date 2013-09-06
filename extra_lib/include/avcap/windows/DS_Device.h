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


#ifndef DSDEVICE_H_
#define DSDEVICE_H_

#include <string>
#include <list>

#include "DS_DeviceDescriptor.h"
#include "CaptureDevice.h"
#include "avcap-export.h"

namespace avcap
{
// forward declarations
class DS_ConnectorManager;
class DS_ControlManager;
class DS_VidCapManager;
class DS_DeviceDescriptor;
class DS_FormatManager;

	//! Implementation of the CaptureDevice for DirectShow.

	class AVCAP_Export DS_Device : public CaptureDevice
	{
	private:
		DS_VidCapManager*		mVidCapMgr;
		DS_ConnectorManager*	mConnectorMgr;
		DS_ControlManager*		mControlMgr;
		DS_FormatManager*		mFormatMgr;
		DS_DeviceDescriptor*	mDSDeviceDescriptor;

	public:
		DS_Device(DS_DeviceDescriptor* dd);

		virtual ~DS_Device();

		inline const DeviceDescriptor* getDescriptor()
			{ return (const DeviceDescriptor*) mDSDeviceDescriptor;	}

		inline CaptureManager* getVidCapMgr()
			{ return (CaptureManager*) mVidCapMgr; }

		inline ConnectorManager* getConnectorMgr()
			{ return (ConnectorManager*) mConnectorMgr;	}

		inline ControlManager* getControlMgr()
			{ return (ControlManager*) mControlMgr;	}

		inline FormatManager* getFormatMgr()
			{ return (FormatManager*)mFormatMgr; }

	private:
		int open();

		int close();
	};
}

#endif // DSDEVICE_H_
