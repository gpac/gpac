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

#ifndef QT_CONTROL_H_
#define QT_CONTROL_H_

#include <string>
#include <list>

#include "Control_avcap.h"

namespace avcap
{
	// forward declaration
	class QT_DeviceDescriptor;
	
	//! Implementation of a Control for the QuickTime-implementation of CaptureDevice.

	/*! The QT API supports only controls for brightness, hue, colour, contrast, depth and whiteness.
	 * They are all integer values between 0 and 65535.*/
	 
	class QT_Control: public IntegerControl
	{
	public:
		enum Ctrl {
			BRIGHTNESS = 0,
			BLACKLEVEL,
			WHITELEVEL,
			HUE,
			CONTRAST,
			SATURATION,
			SHARPNESS
		};
	
	private:	
		static std::string mNames[7];
		
		QT_DeviceDescriptor*	mDescriptor;
		Ctrl					mType;
		unsigned short			mDefaultValue;
		Interval				mInterval;
		
	public:
		QT_Control(QT_DeviceDescriptor* dd, Ctrl type);

		virtual inline ~QT_Control()
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

#endif // QT_CONTROL_H_

