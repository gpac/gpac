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


#ifndef MANAGER_H_
#define MANAGER_H_

#include <list>
#include <iostream>

namespace avcap
{
	class DeviceDescriptor;
	
	//! Abstract base class for Managers. 
	
	/*! Classes that provide access to specific aspects of 
	 * a device derive from this class. Managers usualy manage a number of 
	 * objects of a specific type that abstract these aspects.
	 * The template parameter is used to define a STL list-type to 
	 * store these objects.*/
	 
	template<class T>
	class Manager
	{
	public:
		//! The STL list-type to store the managed objects.
		typedef std::list<T*>	ListType;
		
	protected:
		DeviceDescriptor	*mDeviceDescriptor;
		
	public:
		inline Manager(DeviceDescriptor* dd):
			mDeviceDescriptor(dd) 
			{}
			
		virtual ~Manager() 
			{}
		
		/*! Called during initialisation by the CaptureDevice to query for 
		 * the objects that the implementation of this class manages. */
		virtual void query() = 0;		
	};
}

#endif // MANAGER_H_
