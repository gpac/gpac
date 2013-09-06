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


#ifndef DSDEVICEDESCRIPTOR_H_
#define DSDEVICEDESCRIPTOR_H_

#include <iostream>
#include <string>
#include <DShow.h>

#include "DeviceDescriptor.h"
#include "avcap-export.h"

class IBaseFilter;

namespace avcap
{
class CaptureDevice;
class DS_Device;

	//! Implementation of the DeviceDescriptor for DirectShow.

	class AVCAP_Export DS_DeviceDescriptor : public DeviceDescriptor
	{
	public:
		// Used by AddToRot() and RemoveFromRot() for spying on
		// filter graph with GraphEdit software
		unsigned long mRegister; // For debugging purpose

	private:
		std::string mName;
		std::string mCard;
		std::string mInfo;

		int 			mCapabilities;
		DEV_HANDLE_T 	mHandle;
		bool 			mValid;
		bool			mIsOpen;
		DS_Device*		mDevice;

	public:
		DS_DeviceDescriptor(const std::string &card);

		virtual ~DS_DeviceDescriptor();

		virtual CaptureDevice* getDevice();

		int open();

		int close();

		virtual inline const std::string& getName() const
			{ return mName; }

		// implementation of pure virtual method
		inline const DEV_HANDLE_T getHandle() const
			{ return mHandle; }

		// but non-const access is needed by the device-class
		inline DEV_HANDLE_T getHandle()
			{ return mHandle; }

		bool isAVDev() const;

		bool isVideoCaptureDev() const;

		bool isVBIDev() const;

		bool isTuner() const;

		bool isAudioDev() const;

		bool isRadioDev() const;

		bool isOverlayDev() const;

		//! Device is a VFW (Video for Windows) device.
		/*! \return true VFW device, else false */
		bool isVfWDevice() const;

	private:
		bool queryCapabilities();

		void findDevice(std::string &UniqueDeviceID, IBaseFilter **CaptureFilter,
				std::string &DeviceName, std::string &DeviceInfo,
				bool *IsVideoDevice=NULL, bool *IsAudioDevice=NULL);

		bool createCaptureFilterGraph(std::string &UniqueDeviceID,
				IBaseFilter *CaptureFilter);

		bool getInfosFromDevice(IBaseFilter *CaptureFilter, int *Capabilities,
				std::string &Card, std::string &Info);

		bool findOverlaySupport(IBaseFilter *CaptureFilter, int *Capabilities);

		bool findVBISupport(IBaseFilter *CaptureFilter, int *Capabilities);

		bool findTunerRadioSupport(IBaseFilter *CaptureFilter, int *Capabilities);

		bool isAudioOrVideoDevice(IBaseFilter *CaptureFilter, int *Capabilities);

		HRESULT addToRot(IUnknown *pUnkGraph, DWORD *pdwRegister);

		void removeFromRot(DWORD pdwRegister);

		friend class DS_Device;
	};
}

#endif // DSEVICEDESCRIPTOR_H_
