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

#ifndef V4L1_CONTROL_H_
#define V4L1_CONTROL_H_

#include <string>
#include <list>

#include "Control_avcap.h"

namespace avcap
{
	// forward declaration
	class V4L1_DeviceDescriptor;
	
	//! Implementation of a V4L1-Control.

	/*! The V4L1 API supprots only controls for brightness, hue, colour, contrast, depth and whiteness.
	 * They are all integer values between 0 and 65535.*/
	 
	class V4L1_Control: public IntegerControl
	{
	public:
		enum Ctrl {
			BRIGHTNESS = 0,
			HUE,
			COLOUR,
			CONTRAST,
			WHITENESS,
			DEPTH
		};
	
	private:	
		static std::string mNames[6];
		
		V4L1_DeviceDescriptor*	mDescriptor;
		Ctrl	mType;
		int		mDefaultValue;
		Interval mInterval;
		
	public:
		V4L1_Control(V4L1_DeviceDescriptor* dd, Ctrl type,  __u16 def);

		virtual inline ~V4L1_Control()
			{}
		
		virtual inline int getId() const
			{ return mType; }
		
		virtual inline int getDefaultValue() const
			{ return mDefaultValue; }
		
		virtual const std::string& getName() const;
		
		virtual int setValue(int val);

		virtual int getValue() const;
		
		virtual int reset();

		virtual inline const Interval& getInterval() const 
			{ return mInterval; }
	};
}

#endif // V4L1_CONTROL_H_

