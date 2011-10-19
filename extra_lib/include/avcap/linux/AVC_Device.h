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


#ifdef HAS_AVC_SUPPORT

#ifndef AVC_DEVICE_H_
#define AVC_DEVICE_H_

#include <string>
#include <list>

#include "DeviceDescriptor.h"
#include "CaptureDevice.h"
#include "AVC_FormatManager.h"

namespace avcap
{
	// forward declarations
	class AVC_DeviceDescriptor;
	class AVC_VidCapManager;
	class AVC_ConnectorManager;
	class AVC_ControlManager;

	//! Implementation of the CaptureDevice for IEEE 1394 AV/C-devices under linux.

	/*! Such devices don't have controls, various resolutions, extensions, or connectors.
	 * So the implementation of the methods rely mostly on the default implementations or are
	 * implemented as noop.
	 *
	 * Note: If capturing from AV/C-devices is enabled, avcap depends on
	 * libiec61883, libavc1394, librom1394, libraw1394
	 * Furthermore the user requires read/write access to /dev/raw1394.
	 *
	 * AV/C-support is enabled by the configure-script, if the neccessary libs and developement-headers
	 * are found on the system.
	 *
	 * The AV/C-support is based on dvgrab (http://www.kinodv.org), which is released under the GPLv2.
	 */

	class AVC_Device : public CaptureDevice
	{
	public:

	private:
		AVC_DeviceDescriptor*	mDeviceDescriptor;
		AVC_FormatManager*		mFormatMgr;
		AVC_VidCapManager*		mVidCapMgr;
		AVC_ConnectorManager*	mConnectorMgr;
		AVC_ControlManager*		mControlMgr;

	public:
		//! Constructor
		AVC_Device(AVC_DeviceDescriptor* dd);

		//! Destructor
		virtual ~AVC_Device();

		inline const DeviceDescriptor* getDescriptor()
			{ return mDeviceDescriptor; }

		inline CaptureManager* getVidCapMgr()
			{ return (CaptureManager*) mVidCapMgr; }

		inline ConnectorManager* getConnectorMgr()
			{ return (ConnectorManager*) mConnectorMgr; }

		inline ControlManager* getControlMgr()
			{ return (ControlManager*) mControlMgr; }

		inline FormatManager* getFormatMgr()
			{ return (FormatManager*) mFormatMgr; }

	private:
		int open();

		int close();
	};
}

#endif // AVC_DEVICE_H_
#endif // HAS_AVC_SUPPORT

