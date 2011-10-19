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


#ifndef V4L1_DEVICEDESCRIPTOR_H_
#define V4L1_DEVICEDESCRIPTOR_H_

#include <iostream>

#include "DeviceDescriptor.h"

namespace avcap
{
class CaptureDevice;
class V4L1_Device;

	//! This class uniquely identifies a Video4Linux1 capture device.

	class V4L1_DeviceDescriptor : public DeviceDescriptor
	{
	public:
		struct bounds {
			unsigned int minwidth, minheight, maxwidth, maxheight;
		};

	private:
		std::string	mName;
		std::string mDriver;
		std::string mCard;
		std::string mInfo;
		std::string mVersionString;

		int mVersion;

		DEV_HANDLE_T mHandle;
		bool mValid;

		// Video4Linux1 specific data
		int		mType;
		bounds	mBounds;
		int 	mAudios;
		int		mChannels;
		bool	mIsStreamingDev;

		V4L1_Device*	mDevice;

	public:
		V4L1_DeviceDescriptor(const std::string &name);

		virtual ~V4L1_DeviceDescriptor();

		virtual CaptureDevice* getDevice();

		virtual int open();

		virtual int close();

		virtual const std::string& getName() const;

		inline const std::string& getDriver() const
			{ return mDriver; }

		inline const std::string& getCard() const
			{ return mCard; }

		inline const std::string& getInfo() const
			{ return mInfo; }

		inline int getVersion() const
			{ return mVersion; }

		const std::string& getVersionString() const;

		inline const DEV_HANDLE_T getHandle() const
			{ return mHandle; }

		bool isAVDev() const;

		bool isVideoCaptureDev() const;

		bool isVBIDev() const;

		bool isTuner() const;

		bool isAudioDev() const;

		bool isRadioDev() const;

		bool isOverlayDev() const;

		bool isRWDev () const;

		bool isAsyncIODev() const;

		bool isStreamingDev() const;

		inline int getChannels() const
			{ return mChannels; }

		inline int getAudios() const
			{ return mAudios; }

		//! Returns a reference to the bounds of the video size.
		inline bounds& getBounds()
			{ return mBounds; }

	private:
		bool queryCapabilities();
	};
};

#endif // V4L1_DEVICEDESCRIPTOR_H_
