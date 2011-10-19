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


#ifndef QT_DEVICEDESCRIPTOR_H_
#define QT_DEVICEDESCRIPTOR_H_

#include <iostream>
#include <string>

#include "DeviceDescriptor.h"
#include <QuickTime/QuickTime.h>

namespace avcap
{
class CaptureDevice;
class QT_Device;

	//! Implementation of the DeviceDescriptor for QuickTime.

	class QT_DeviceDescriptor : public DeviceDescriptor
	{
	public:

	private:
		SeqGrabComponent mGrabber;
		SGChannel		 mChannel;
		VideoDigitizerComponent mDigitizer;
		DigitizerInfo			mDigiInfo;

		int				 mDeviceID;
		int				 mInputID;
		bool			 mValid;

		std::string		 mName;
		std::string		 mDriver;
		QT_Device*		 mDevice;

	public:
		QT_DeviceDescriptor(int device, int input, const std::string& dev_name, const std::string& driver_name,
			SeqGrabComponent current_grabber, SGChannel current_channel);

		QT_DeviceDescriptor();

		virtual ~QT_DeviceDescriptor();

		virtual CaptureDevice* getDevice();

		virtual int open();

		virtual int close();

		virtual const std::string& getName() const;

		virtual const std::string& getDriver() const;

		bool isVideoCaptureDev() const;

		virtual inline const DEV_HANDLE_T getHandle() const
			{ return 0; }

		//! Get the SequenceGrabber-Component.
		/*! \return the SequenceGrabber-Component. */
		inline SeqGrabComponent getGrabber(void)
			{ return mGrabber; }

		//! Get the SequenceGrabber-Channel associated with the device.
		/*! \return the SequenceGrabber-Channel. */
		inline SGChannel getChannel(void)
			{ return mChannel; }

		//! Get the VideoDigitizer associated with the device.
		/*! \return the VideoDigitizer. */
		inline VideoDigitizerComponent getDigitizer(void)
			{ return mDigitizer; }

	private:
		bool queryCapabilities(SeqGrabComponent current_grabber, SGChannel current_channel);

	};
}

#endif // QT_DEVICEDESCRIPTOR_H_
