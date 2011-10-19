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

#ifndef DEVICECOLLECTOR_H_
#define DEVICECOLLECTOR_H_

#include <list>
#include <string>

#include "singleton.h"

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include "avcap-config.h"
#endif

#include "avcap-export.h"

namespace avcap
{
class DeviceDescriptor;
class CaptureDevice;

	//! This singleton queries the capture devices available on the system and provides a factory-method to create CaptureDevice-objects.
	
	/*! This class tests during instantiation (i.e. the first call to it's instance()-method), 
	 * which capture devices are available on the system
	 * and provides an STL-list of DeviceDescriptor objects describing these devices.  
	 * 
	 * The following strategy to find capture devices in the system is applied:
	 *  
	 * <b>Linux:</b>
	 * <UL> 
	 * <LI>All /dev/video* nodes are tested by default. </LI>
	 * <LI>If avcap has been compiled with HAS_AVC_SUPPORT defined, all IEEE 1394 AV/C-devices are tested. </LI>
	 * <LI>IEEE1394 digital camera support is planned but currently not implemented. 
	 * </UL>
	 * <b>Win32:</b>
	 * <UL> 
	 * <LI>All devices in the CLSID_VideoInputDeviceCategory category (see DirectShow documentation) are tested.</LI>
	 * </UL>
	 * <b>Darwin:</b>
	 * <UL> 
	 * <LI>All devices of the SequenceGrabber-Component device-list are tested.</LI>
	 * </UL>
	 * 
	 * Access the singleton instance via DEVICE_COLLECTOR::instance().
	 **/
	
	class AVCAP_Export DeviceCollector
	{
	public:
		//! List type of the DeviceDescriptor object list.
		typedef std::list<DeviceDescriptor*> DeviceList;
	
	private:
		DeviceList mDeviceList;
	
	public:
		//! Constructor
		DeviceCollector();
		
		//! Destructor
		virtual ~DeviceCollector();
	
		//! Returns the STL-list of DeviceDescriptor objects describing available capture devices.
		/*! \return The descriptor list.*/
		inline const DeviceList& getDeviceList() const 
			{ return (const DeviceList&) mDeviceList; }
	
		//! Linux only! Test, if the device with the given name can be opened and is a V4L1 or V4L2 capture device or not. 
		/*! If it is, a new DeviceDescriptor-object is created 
		 * and stored in the device list, managed by the collector. 
		 * \param name : the name of a device node (e.g. /dev/video0) 
		 * \return true, if it is a V4L1-device, false else*/
		bool testDevice(const std::string& name);
	
	private:
	
#ifdef AVCAP_LINUX
		void query_V4L1_Devices();
		
		void query_V4L2_Devices();
		
		void query_ieee1394_Devices();
		
		int test_V4L1_Device(const std::string& name);
		
		int test_V4L2_Device(const std::string& name);
#endif

#ifdef AVCAP_OSX
		void query_QT_Devices();
#endif

#ifdef AVCAP_WINDOWS
		void query_DS_Devices();
		
		int test_DS_Device(const std::string& name);
		
		bool getInstalledDeviceIDs(std::list<std::string> &UniqueDeviceIDList);

#endif
};

//! The DeviceCollector singleton. Access the singleton instance via DEVICE_COLLECTOR::instance().
typedef Singleton<DeviceCollector> DEVICE_COLLECTOR;
}

/* \todo Add Linux-support for IEEE 1394 digital cameras. */

#endif // DEVICECOLLECTOR_H_
