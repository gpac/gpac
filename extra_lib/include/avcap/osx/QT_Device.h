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

#ifndef QT_DEVICE_H
#define QT_DEVICE_H

#include <string>
#include <list>

#include "DeviceDescriptor.h"
#include "CaptureDevice.h"
#include "QT_FormatManager.h"

namespace avcap
{
	// forward declarations
	class QT_DeviceDescriptor;
	class QT_VidCapManager;
	class QT_ConnectorManager;
	class QT_ControlManager;

	//! Implementation of the CaptureDevice for QuickTime.

	class QT_Device : public CaptureDevice
	{
	public:

	private:
		QT_DeviceDescriptor*	mDeviceDescriptor;
		QT_FormatManager*		mFormatMgr;
		QT_VidCapManager*		mVidCapMgr;
		QT_ConnectorManager*	mConnectorMgr;
		QT_ControlManager*		mControlMgr;

	public:
		QT_Device(QT_DeviceDescriptor* dd);

		virtual ~QT_Device();

		inline const DeviceDescriptor* getDescriptor()
			{ return (const DeviceDescriptor*) mDeviceDescriptor; }

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

#endif // QT_DEVICE_H

