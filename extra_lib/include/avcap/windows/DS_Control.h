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


#ifndef DSCONTROL_H_
#define DSCONTROL_H_

#include <string>

#include "Control_avcap.h"
#include "Interval.h"
#include "avcap-export.h"

namespace avcap
{
class DS_DeviceDescriptor;

	//! Implementation of the Control-class for DirectShow.

	class AVCAP_Export DS_Control
	{
	public:
		enum Ctrl_Type
		{
			CTRL_TYPE_INTEGER = 1,
			CTRL_TYPE_BOOLEAN = 2,
		};
	
		enum DShowInterface
		{
			IAM_VIDEOPROCAMP = 1,
			IAM_CAMERACONTROL = 2,
		};
	
		//! Helper-class to query control-properties
		
		struct Query_Ctrl
		{
			unsigned int 	id;
			Ctrl_Type 		type;
			std::string 	name;
			
			long minimum;
			long maximum;
			long step;
			long default_value;
			long flags;
			long property;
	
			DS_Control::DShowInterface DShowInterfaceType;
		};
	
	private:
		DShowInterface 	mDShowInterfaceType;
		long			mProperty;
		int 			mId;
		int 			mValue;
		std::string 	mName;
		int 			mDefaultValue;
		int 			mFlags;
		bool 			mInitialized;
	
	protected:
		DS_DeviceDescriptor* mDSDeviceDescriptor;
	
	public:
		DS_Control(DS_DeviceDescriptor *dd, Query_Ctrl* query);
		virtual ~DS_Control();
	
		inline int getId() const
			{ return mId; }
	
		inline int getDefaultValue() const
			{ return mDefaultValue; }
	
		inline const std::string& getName() const
			{ return mName;	}
	
		int setValue(int val);
	
		int getValue() const;
	
		int reset();
		
	private:
		int update();

		int retrieveValue();
	};
	
	//! DirectShow integer valued control.
	
	class DS_IntControl : public IntegerControl
	{
	private:
		Interval 	mInterval;
		DS_Control	mControlBase;
		
	public:
		inline DS_IntControl(DS_DeviceDescriptor *dd, struct DS_Control::Query_Ctrl* query) :
			mInterval(query->minimum, query->maximum, query->step),
			mControlBase(dd, query)
			{}
	
		virtual inline ~DS_IntControl()
			{}
	
		inline const Interval& getInterval() const 
			{ return mInterval; }

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
	};
	
	//! DirectShow boolean control.

	class DS_BoolControl : public BoolControl
	{
	private:
		DS_Control	mControlBase;

	public:
		inline DS_BoolControl(DS_DeviceDescriptor *dd, struct DS_Control::Query_Ctrl* query) :
			mControlBase(dd, query)
			{}
	
		virtual inline ~DS_BoolControl()
			{}
		
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
	};
}

#endif // DSCONTROL_H_
