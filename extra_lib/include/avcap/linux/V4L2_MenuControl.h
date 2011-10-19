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

#ifndef V4L2_MENUCONTROL_H_
#define V4L2_MENUCONTROL_H_

#include "V4L2_ControlBase.h"
#include "Control_avcap.h"

namespace avcap
{
	// forward declaration
	class V4L2_DeviceDescriptor;
	
	//! A Control for a Video4Linux2 device which has various menu-like items to choose from.
	
	class V4L2_MenuControl: public MenuControl
	{
	private:
		V4L2_ControlBase	mControlBase;
		ItemList 			mMenuItems;
		V4L2_DeviceDescriptor*	mDeviceDescriptor;
		
	public:
		//! The constructor.
		V4L2_MenuControl(V4L2_DeviceDescriptor *dd, struct v4l2_queryctrl* query);

		//! The destructor.
		virtual ~V4L2_MenuControl();
	
		virtual inline int getId() const
			{ return mControlBase.getId(); }
	
		virtual inline int getDefaultValue() const
			{ return mControlBase.getDefaultValue(); }
	
		virtual inline const std::string& getName() const
			{ return mControlBase.getName(); }
	
		virtual inline int setValue(int val)
			{ return mControlBase.setValue(val); }
	
		virtual inline int getValue() const
			{ return mControlBase.getValue(); }
		
		virtual inline int reset() 
			{ return mControlBase.reset(); }

		virtual inline const ItemList& getItemList () 
			{ return mMenuItems; }

	private:
		void queryMenuItems();
	};
}

#endif //V4L2_MENUCONTROL_H_
