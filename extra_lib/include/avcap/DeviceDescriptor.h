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


#ifndef DEVICEDESCRIPTOR_H_
#define DEVICEDESCRIPTOR_H_

#include <iostream>
#include <string>

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include "avcap-config.h"
#endif

#include "avcap-export.h"

// IVTV driver name
#define DRIVER_IVTV	"ivtv"

namespace avcap
{
class CaptureDevice;

	//! Objects of classes derived from this abstract base uniquely identify a capture device in a system.

	/*! It is used as an system independent description of a capture device.
	 * It provides the interface to access information about a device and the device itself.
	 * Special devices must inherit this class, e.g. V4L2_DeviceDescriptor or AVC_DeviceDescriptor.
	 * A list of objects derived from this class (one for each device) is provided
	 * by the DeviceCollector-singleton, which tries
	 * to determine all capture devices available on the system, so applications
	 * don't have to instantiate objects of these class explicitly. Objects of this
	 * class can be used to create a concrete CaptureDevice object by calling DEVICE_COLLECTOR::instance()->createDevice().
	 * The class must be implemented for a concrete capture API/OS.
	 */

	class AVCAP_Export DeviceDescriptor
	{
	public:
#ifdef AVCAP_LINUX
		typedef int DEV_HANDLE_T;
#endif

#ifdef AVCAP_OSX
		typedef int DEV_HANDLE_T;
#endif

#ifdef _WIN32
		//! <b>Win32:</b> Platform dependent device handle type for windows. (represents the DirectShow capture filter (always casted to a IBaseFilter COM-Interface).
		/* To get the complete created filter graph call GetFilterGraphFromFilter()
		 declared in the "HelpFunc.h" header file. */
		typedef void* DEV_HANDLE_T;
#endif

	private:
		const static std::string mEmptyString;

	public:
		//! Constructor
		DeviceDescriptor();

		//! Destructor
		virtual ~DeviceDescriptor() = 0;

		//! Open the underlying device.
		/*! The CaptureDevice-Object returned by getDevice(), which is actually used to perform
		 * capturing is not valid before open() is called.
		 * \return 0 success, -1 on failure, e.g. open() has been already called before
		 */
		virtual int open() = 0;

		//! Close the underlying device.
		/*! The CaptureDevice-Object returned by getDevice(), which is actually used to perform
		 * capturing, is not valid after close() is called.
		 * \return 0 success, -1 failure
		 */
		virtual int close() = 0;

		//! Returns the unique identifier of the device.
		/*! \return unique identifier of device */
		virtual const std::string& getName() const = 0;

		//! Returns the name of the driver.
		/*! The default implementation returns an empty string.
		 * \return driver */
		virtual inline const std::string& getDriver() const
			{ return mEmptyString; }

		//! Returns the name of the device
		/*! The default implementation returns an empty string.
		 * \return name of the card */
		virtual inline const std::string& getCard() const
			{ return mEmptyString; }

		//! Returns a textual description of the device.
		/*! The default implementation returns an empty string.
		 * \return name */
		virtual inline const std::string& getInfo() const
			{ return mEmptyString; }

		//! Returns the version number of the driver.
		/*! The default implementation returns 0.
		 * \return version. */
		virtual inline int getVersion() const
			{ return 0; }

		//! Returns the version number of the driver as string.
		/*! The default implementation returns an empty string.
		 * \return version string. */
		virtual inline const std::string& getVersionString() const
			{ return mEmptyString; }

		//! Returns the API-specific device handle used to reference the device.
		/*! \return the device handle */
		virtual const DEV_HANDLE_T getHandle() const = 0;

		//! Device is an audio/video device. The default implementation returns false.
		virtual inline bool isAVDev() const
			{ return false; }

		//! Device is capable to capture some data. The default implementation returns false.
		virtual inline bool isVideoCaptureDev() const
			{ return false; }

		//! Device is a VBI device. The default implementation returns false.
		virtual inline bool isVBIDev() const
			{ return false; }

		//! Device has a tuner. The default implementation returns false.
		virtual inline bool isTuner() const
			{ return false; }

		//! Device is an audio device. The default implementation returns false.
		virtual inline bool isAudioDev() const
			{ return false; }

		//! Device is a radio device. The default implementation returns false.
		virtual inline bool isRadioDev() const
			{ return false; }

		//! Device supports video overlay. The default implementation returns false.
		virtual inline bool isOverlayDev() const
			{ return false; }

		//! Device supports read/write IO-methods (linux specific, see V4L2 API Docu for further details).
		/*!  The default implementation returns false. */
		virtual inline bool isRWDev () const
			{ return false; }

		//! Device supports asynchroneous IO-methods (linux specific, see V4L2 API Docu for further details).
		/*! The default implementation returns false. */
		virtual inline bool isAsyncIODev() const
			{ return false; }

		//! Device supports memory mapping IO-methods (linux specific, see V4L2 API Docu for further details).
		/*! The default implementation returns false. */
		virtual inline bool isStreamingDev() const
			{ return false; }

		//! Factory-method to create a API-dependent CaptureDevice-object.
		/*! Applications must not create their own
		 * instances of a CaptureDevice but use this method to access the proper
		 * API-dependent unique device-object. You can use this object anywhere between
		 * successive calls to open() and close(), i.e. it is not valid before open() and not after close().
		 * The ownership of the object remains at the descriptor, so the caller
		 * must not delete the object after usage.
		 * Only one CaptureDevice-object will be created for each DeviceDescriptor, so
		 * multiple calls to getDevice() will always return the same object instance.
		 *
		 * \return the CaptureDevice-instance or 0, if not available. */
		virtual CaptureDevice* getDevice() = 0;
	};
}


#endif //DEVICEDESCRIPTOR_H_
