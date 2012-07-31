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

#ifndef V4L2_CONTROLBASE_H_
#define V4L2_CONTROLBASE_H_

#include <string>
#include <list>

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include "avcap-config.h"
#endif

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

namespace avcap
{
	// forward declaration
	class V4L2_DeviceDescriptor;
	
	//! Basic implementation of a device control for a Video4Linux2 device.
	
	/*! Implements common methods of V4L2-controls. */
	
	class V4L2_ControlBase
	{
	protected:
		V4L2_DeviceDescriptor	*mDeviceDescriptor;
		int	mId;
		int mValue;
	
	private:
		std::string	mName;
		int mDefaultValue;
		int mFlags;

	public:
		V4L2_ControlBase(V4L2_DeviceDescriptor *dd, struct v4l2_queryctrl* query);

		virtual ~V4L2_ControlBase();

		inline int getId() const
			{ return mId; }

		inline int getDefaultValue() const
			{ return mDefaultValue; }
		
		inline const std::string& getName() const 
			{ return mName; }
		
		virtual int setValue(int val);

		virtual int getValue() const;
		
		virtual int reset();
		
		//! Return the flags of the v4l2_queryctrl structure associated with the control.
		__u32 getFlags() const { return mFlags; }
		
	protected:
		// updates the value of the control
		int update();
	};
}

#endif // V4L2_CONTROLBASE_H_

