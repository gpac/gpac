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

#ifndef V4L2_CTRLCLASS_CONTROL_H_
#define V4L2_CTRLCLASS_CONTROL_H_

#include "V4L2_ControlBase.h"

namespace avcap
{
	class V4L2_DeviceDescriptor;

	//! A control for a Video4Linux2 device naming the control-class of the subsequent enumerated controls.
	
	class V4L2_CtrlClassControl: public CtrlClassControl
	{
	private:
		V4L2_ControlBase	mControlBase;
		
	public:
		inline V4L2_CtrlClassControl(V4L2_DeviceDescriptor *dd, struct v4l2_queryctrl* query):
			mControlBase(dd, query)
			{}

		virtual inline ~V4L2_CtrlClassControl()
			{}
		
		virtual inline int getId() const
			{ return mControlBase.getId(); }
	
		virtual inline int getDefaultValue() const
			{ return 0; }
	
		virtual inline const std::string& getName() const
			{ return mControlBase.getName(); }
	
		virtual inline int setValue(int val)
			{ return 0; }
	
		virtual inline int getValue() const
			{ return 0; }
		
		virtual inline int reset()
			{ return 0; }
	};
}

#endif // V4L2_CTRLCLASS_CONTROL_H_
