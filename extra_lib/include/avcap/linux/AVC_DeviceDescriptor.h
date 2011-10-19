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


#ifndef AVC_DEVICEDESCRIPTOR_H_
#define AVC_DEVICEDESCRIPTOR_H_

#ifdef HAS_AVC_SUPPORT

#include <iostream>
#include <libavc1394/rom1394.h>

#include "DeviceDescriptor.h"

namespace avcap
{
class CaptureDevice;
class AVC_Device;

	//! This class implements a descriptor for a IEEE 1394 AV/C capture device under linux (e.g. a DV-Camera).

	class AVC_DeviceDescriptor : public DeviceDescriptor
	{
	private:
		std::string mName;
		std::string mGUIDString;
		std::string mInfo;
		std::string	mDriver;

		octlet_t	mGUID;
		static int	mDevCount;
		AVC_Device*	mDevice;

	public:
		//! This constructor uses a numerical global unique identifier to represent a IEEE 1394 capture device (e.g. a firewire DV-Cam).
		/*! \param guid The unique identifier of device.*/
		AVC_DeviceDescriptor(const octlet_t guid);

		//! The destructor */
		virtual ~AVC_DeviceDescriptor();

		int open();

		int close();

		//! Returns the unique identifier of the device. AV/C-devices get the name
		/*! "AV/C_n", where n is the number of the device in the system starting with 1.
		 * So the 3rd device found has the name AV/C_3.
		 * \return unique identifier of device */
		inline const std::string& getName() const
			{ return mName; }

		virtual inline const std::string& getInfo() const
			{ return mInfo; }

		virtual inline const std::string& getDriver() const
			{ return mDriver; }

		virtual inline const std::string& getCard() const
			{ return mGUIDString; }

		//! There is no handle associated with a AV/C-device. So this method always returns -1.
		inline const DEV_HANDLE_T getHandle() const
			{ return -1; }

		//! Device is an audio/video device. Always returns true.
		inline bool isAVDev() const
			{ return true; }

		//! Device is capable to capture some data. Always returns true.
		inline bool isVideoCaptureDev() const
			{ return true; }

		inline octlet_t& getGUID()
			{ return mGUID; }

		virtual CaptureDevice* getDevice();
	};
}

#endif // AVC_DEVICEDESCRIPTOR_H_
#endif // HAS_DVLIBS

