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
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>
 */

#ifndef V4L1_DEVICE_H_
#define V4L1_DEVICE_H_

#include <string>
#include <list>

#include "V4L1_DeviceDescriptor.h"
#include "CaptureDevice.h"

namespace avcap
{
	// forward declarations
	class V4L1_ConnectorManager;
	class V4L1_ControlManager;
	class V4L1_VidCapManager;
	class V4L1_DeviceDescriptor;
	class V4L1_FormatManager;

	//! Implementation of the CaptureDevice interface for Video4Linux2 devices. */

	class V4L1_Device : public CaptureDevice
	{
	public:

	private:
		V4L1_VidCapManager		*mVidCapMgr;
		V4L1_ConnectorManager	*mConnectorMgr;
		V4L1_ControlManager		*mControlMgr;
		V4L1_FormatManager		*mFormatMgr;
		V4L1_DeviceDescriptor	*mDeviceDescriptor;

	public:
		V4L1_Device(V4L1_DeviceDescriptor* dd);

		virtual ~V4L1_Device();

		inline const DeviceDescriptor* getDescriptor()
			{ return mDeviceDescriptor; }

		inline CaptureManager* getVidCapMgr()
			{ return (CaptureManager*) mVidCapMgr; }

		inline ConnectorManager* getConnectorMgr()
			{ return (ConnectorManager*) mConnectorMgr; }

		inline ControlManager*	getControlMgr()
			{ return (ControlManager*) mControlMgr; }

		inline FormatManager* getFormatMgr()
			{ return (FormatManager*) mFormatMgr; }

	private:
		int open();

		int close();
	};
}

#endif //V4L1_DEVICE_H_

