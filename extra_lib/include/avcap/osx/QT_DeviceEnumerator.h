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


#ifndef QT_DEVICEENUMERATOR_H_
#define QT_DEVICEENUMERATOR_H_

#include <QuickTime/QuickTime.h>

#include "DeviceCollector.h"
#include "QT_DeviceDescriptor.h"

namespace avcap
{
	//! This special DeviceDescriptor is used by the DeviceCollector to enumerate QuickTime capture-devices.

	class QT_DeviceEnumerator: public QT_DeviceDescriptor
	{
	private:
		SGDeviceList	mDeviceList;
		
	public:
		QT_DeviceEnumerator();
		virtual ~QT_DeviceEnumerator();
		
		//! Use this method to populate a DeviceList with the descriptors of the Devices found on the system.
		/*! \param dev_list : the list that is filled with DeviceDescriptor-objects.
		 * \return 0 on success, -1 else */ 
		int findDevices(DeviceCollector::DeviceList& dev_list);
		
		virtual int open();
		
		virtual int close();
	};	
}

#endif // QT_DEVICEENUMERATOR_H_

