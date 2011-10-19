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

#ifndef CONTROL_H_
#define CONTROL_H_

#include <string>
#include <list>

#include "avcap-export.h"
#include "Interval.h"

namespace avcap {
// forward declaration
class DeviceDescriptor;

	//! Abstract Base class for all device controls.
	
	/*! Capture devices possess various controls (e.g. hue, saturation,...) of different type.
	 * This class provides the interface that all controls share. Objects 
	 * derived from this class are managed by a ControlManager which is 
	 * obtained by the concrete CaptureDevice object. 
	 * A concrete control may expose an extended interface to provide additional functionality. 
	 * Applications can use the getType()-method or RTTI to determine the type of the concrete control. */

	class AVCAP_Export Control
	{
	public:
		enum Type {
			INTEGER_CONTROL = 0,
			BOOL_CONTROL,
			BUTTON_CONTROL,
			MENU_CONTROL,
			CTRLCLASS_CONTROL,
			USERDEFINED_CONTROL
		};
		
	private:
		Type mType;
		
	public:
		//! Constructor
		Control(Type t): mType(t)
			{}
	
		//! Destructor
		virtual ~Control() 
			{}
		
		//! Get the unique identifier of the control.
		/*! \return id */
		virtual int getId() const = 0;
	
		//! Get the default value of the control.
		/*! \return default value */
		virtual int getDefaultValue() const = 0;
	
		//! Get the name of the control.
		/*! \return control name */
		virtual const std::string& getName() const = 0;
	
		//! Set the new value of the control.
		/*!  \param val : The new value.
		 * \return 0, if successful, -1 else */
		virtual int setValue(int val) = 0;
	
		//! Get the current value of the control.
		/*!  \return the value */
		virtual int getValue() const = 0;
	
		//! Set the  value of the control to the default value.
		/*!  \return 0, if successful, -1 else */
		virtual int reset() = 0;
		
		//! Return the type of the control.
		/* \return type */  
		virtual inline Type getType() const 
			{ return mType; }
		
	private:
		Control() 
			{}
	};
	
	//! Abstraction of an Integer-valued control.
	/*! Such controls additionally provide information about the range and step of it's values. */
	
	class AVCAP_Export IntegerControl: public Control
	{
	public:
		IntegerControl(): Control(Control::INTEGER_CONTROL)
			{}
		
		virtual ~IntegerControl() 
			{}
		
		//! Get the interval describing the range and step of valid values for this control.
		/*! \return interval */
		virtual const Interval& getInterval() const = 0; 
	};
	
	//! Abstraction of a boolean-like control.
	/*! Such controls provide no additional information. */
	
	class AVCAP_Export BoolControl: public Control
	{
	public:
		BoolControl(): Control(Control::BOOL_CONTROL)
			{}
		
		virtual ~BoolControl() 
			{}	
	};

	//! Abstraction of a button-like control.
	/*! Such controls provide a convinience-method to trigger the buttons action. */

	class AVCAP_Export ButtonControl: public Control
	{
	public:
		ButtonControl(): Control(Control::BUTTON_CONTROL)
			{}
		
		virtual ~ButtonControl() 
			{}
		
		//! Push the button. 
		/*! \return 0, if successful, -1 else */
		virtual int push() = 0;
	};

	//! Abstraction of a control describing the class of the successive controls.
	/*! The sole purpose of such a control is to provide a name for the class of the controls
	 * following in the control-list. This name can be used for example to group the controls 
	 * in a 'tabbed' user-interface (See V4L2-API spec, Extended Controls).  */
	
	class AVCAP_Export CtrlClassControl: public Control
	{
	public:
		CtrlClassControl(): Control(Control::CTRLCLASS_CONTROL)
			{}
		
		virtual ~CtrlClassControl() 
			{}
	};

	
	//! A menu item.
	struct MenuItem
	{
		//! The name of the item.
		std::string name;
		
		//! The index to identify the item.
		int index;

	public:
		//! The constructor.
		inline MenuItem(const std::string& n, int i):
			name(n), index(i)
			{}
	};

	//! Abstraction of a menu-like control.
	/*! These controls provide a list of items, the user can chose from. */
	
	class AVCAP_Export MenuControl: public Control
	{
	public:
		//! Type of the item list.
		typedef std::list<MenuItem*> ItemList;

	public:
		MenuControl(): Control(Control::MENU_CONTROL)
			{}
		
		virtual ~MenuControl() 
			{}
		
		//! Returns the STL-list of menu items associated with this control.
		/*! \return the menu items. */
		virtual const ItemList& getItemList () = 0;
	};
}

#endif // CONTROL_H_
