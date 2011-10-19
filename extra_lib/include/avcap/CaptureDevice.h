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


#ifndef CAPTUREDEVICE_H_
#define CAPTUREDEVICE_H_

#include <string>
#include <list>

#include "avcap-export.h"
#include "DeviceDescriptor.h"

//! This is the namespace which contains all classes of the avcap-library.

namespace avcap
{
	// forward declarations
	class ConnectorManager;
	class ControlManager;
	class CaptureManager;
	class DeviceDescriptor;
	class FormatManager;

	/*! \brief This class is an abstraction of a video capture device.
	 *
	 * It is the main entry point for an application to access the functionality of
	 * a capture device, i.e. to query capabilities and settings of the device, to
	 * manipulate them and to capture the video.
	 *
	 * The access to the functionality of a capture device is divided into different domains and is
	 * provided by so called managers. See the documentation of the various Manager-classes for
	 * the details of their usage. The managers and their responsibilities are:
	 *
	 * <ul>
	 * <li> CaptureManager: start/stop capture and register a capture-handler </li>
	 * <li> ConnectorManager: query available inputs/outputs of the device and get/set them</li>
	 * <li> ControlManager: query available controls of the device and get/set their values </li>
	 * <li> FormatManager: query and get/set available formats,
	 * 	their properties and resolutions associated with them </li>
	 * </ul>
	 *
	 * CaptureDevice-classes implementing the back-end for a certain capture-API must derive from this class and
	 * implement the abstract methods to provide the API-specific manager-classes.
	 * However, if you want to use avcap only, you can use one of the following implementations,
	 * representing the supported, existing devices (depending on the operating system avcap has
	 * been build on, not all of them may be available and/or not all methods of their managers are implemented):
	 *
	 * <ul>
	 * <li> V4L2_Device (Linux): devices that are supported by a Video4Linux2-API driver
	 * (Requires read/write access to the /dev/video* -file of the device)</li>
	 * <li> V4L1_Device (Linux): devices that are supported by a Video4Linux-API driver
	 * (Requires read/write access to the /dev/video* -file of the device)</li>
	 * <li> AVC_Device (Linux): support for AV/C-Devices (e.g. DV-Cams).
	 * (Requires: libiec61883, libavc1394, librom1394, libraw1394 and read/write access to /dev/raw1394)</li>
	 * <li> QT_Device (MAC OS X): capture from a device using the QuickTime SampleGrabber </li>
	 * <li> DS_Device (Win32): capture from a device using a DirectShow filter-graph</li>
	 * </ul>
	 *
	 * A concrete CaptureDevice-object must not be created manually. A unique instance can be obtained by calling
	 * the method DeviceDescriptor::getDevice() between successive calls to DeviceDescriptor::open() and close().
	 * All available DeviceDescriptors representing the capture devices found on a system can be obtained by calling
	 * DeviceCollector::instance()->getDeviceList(). The CaptureDevice-object is owned by the DeviceDescriptor,
	 * so you must not delete the CaptureDevice instance.
	 *
	 */

	class AVCAP_Export CaptureDevice
	{
	public:
		//! Constructor
		inline CaptureDevice()
			{}

		//! Destructor
		virtual inline ~CaptureDevice()
			{}

		//! Return the descriptor of the device.
		/*! \return The DeviceDescriptor-object.*/
		virtual const DeviceDescriptor* getDescriptor() = 0;

		//! Use this manager to start/stop capturing and to register a user defined CaptureHandler.
		/*! \return The VidCapManager.*/
		virtual CaptureManager* getVidCapMgr() = 0;

		//! Use this manager to query available audio/video inputs/outputs and to select them.
		/*! \return The ConnectorManager. */
		virtual ConnectorManager* getConnectorMgr() = 0;

		/*! Use this manager to query and to adjust the available controls of the device (e.g.
		 * brightness, contrast, saturation...).
		 * \return The ControlManager. */
		virtual ControlManager*	getControlMgr() = 0;

		//! Use this manager to query the available formats, video standards and resolutions to select the desired ones.
		/*! \return The FormatManager. */
		virtual FormatManager* getFormatMgr() = 0;

	private:
		//! Open the device and do initialization. May fail, if already opened before.
		/*! This method creates the managers. Don't use them before open() has been called.
		 * \return 0 if successful, -1 else
		 * */
		virtual int open() = 0;

		//! Close the device and do cleanup.
		/*! All IOBuffers received by a capture-CallbackHandler should have called their
		 * release()-method, before close() is called to ensure propper cleanup. Usually this is guaranteed,
		 * if the buffer is released in the context of the capture-thread from within handleCapture().
		 * All managers are destroyed by this method and are thus not available anymore after calling close().
		 * \return 0 if successful, -1 else */
		virtual int close() = 0;
	};
}

#endif // CAPTUREDEVICE_H_

/*! \mainpage avcap-library
 *
 * \section intro Introduction
 *
 * The avcap-library is a cross-API, cross-platform simple and easy to use C++
 * video capture library. It's aim is to provide a unified API for
 * Linux, Windows and Mac OS X to capture video from appropriate hardware. It hides the
 * system specific quirks and issues of different API's used on different systems to access video
 * capture hardware and hopefully helps to write portable capture-applications.
 *
 * \subsection linux Linux
 * Under GNU/Linux the avcap-library supports Video4Linux-Devices, Video4Linux2-Devices and AV/C-Devices (e.g.
 * DV-Cams) as capture sources. Note that you need read/write permission to the /dev/video* files
 * to use V4L(2)-Devices. Usually it is sufficient, if the user is a member of the group that owns this files
 * (usually group 'video'). To capture from AV/C-Devices the user needs read/write permission to /dev/raw1394. Membership
 * in the group 'disk' should be sufficient here.
 *
 * \subsection windows Win32 (Implementation by Robin Luedtke, Nico Pranke)
 * The Windows-version is basically a class wrapper for the DirectShow API and thus
 * supports only devices with a WDM (Windows driver model) or an old VFW (Video for
 * windows) compliant capture device driver. Understanding the avcap Win32 implementation
 * may be a little difficult because of the following reasons:
 * First, DirectShow is based on the Windows COM (component object model), second,
 * in some cases, DirectShow is a little confusing (e.g. some DirectShow functions have a strange behavior -- workarounds
 * are inevitable).
 * In addition to this, VFW, WDM and even WDM devices itself are handled differently by DirectShow.
 * Third, some important documentation is missing in the DirectShow documentation.\n \n
 *
 * \subsection mac Mac OS X
 *
 * The implementation for Darwin uses the QuickTime SequenceGrabber-Component and has been tested with the built-in
 * iSight, various USB-cams and DV-Cams.
 *
 * \section install Building and Installation
 *
 * See the INSTALL file.
 *
 * \section Usage
 *
 * For an example on how to use the avcap-library take a look at the captest-program and read the documentation of
 * class CaptureDevice to have a good starting point.
 *
 * \section licence Licence
 *
 * (c) 2005-2008 Nico Pranke <Nico.Pranke@googlemail.com>, Win32 implementation by Robin Luedtke <RobinLu@gmx.de> \n\n
 *
 * For non-commercial use, avcap is distributed under the GNU General Public License version 2. Refer to the file "COPYING" for details.
 *
 * For commercial use, please contact Nico Pranke <Nico.Pranke@googlemail.com> for licensing.
*/

