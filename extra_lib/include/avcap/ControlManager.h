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

#ifndef CONTROLMANAGER_H_
#define CONTROLMANAGER_H_

#include <string>
#include <list>

#include "Control_avcap.h"
#include "Manager.h"
#include "avcap-export.h"

namespace avcap
{
	//! Abstract base for classes that manage the controls of a capture device.
	
	/*! Devices have typically a number of user-setable controls (e.g. brightness, hue,...).
	 * The number of controls, the type and possible values will vary from device to device.
	 * The ControlManager queries for available controls, their type and valid values.
	 * It provides a STL-List of Control-derived objects which represents the functonality of a
	 * device control. The concrete ControlManager may not be instantiated
	 * by the application but can be obtained from the CaptureDevice object. */
	
	class AVCAP_Export ControlManager:public Manager<Control>
	{
	protected:
		ListType mControls;
	
	public:
		//! The constructor. 
		/*! \param dd The device descriptor to access the device. */
		ControlManager(DeviceDescriptor *dd);
	
		//! The destructor.
		virtual ~ControlManager() = 0;
	
		//! Find a control by name.
		/*! \param name The name of the control to find.
		 * \return Pointer to the control or 0, if no control was found. */
		Control* getControl(const std::string& name);
	
		//! Find a control by id.
		/*! \param id The id of the control to find.
		 * \return Pointer to the control or 0, if no control was found. */
		Control* getControl(int id);
	
		//! Returns the STL-list of Control objects.
		/*! \return The control list. */
		inline const ListType& getControlList() 
			{ return (const ListType&) mControls; }
	
		//! Reset all controls to their default values,i.e. calls the reset()-method of all managed controls.
		/*! \return 0 if successful, -1 else */
		virtual int resetAll();
	
		virtual void query() = 0;
	};
}


#endif //CONTROLMANAGER_H_
