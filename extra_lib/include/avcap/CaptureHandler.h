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

#ifndef CAPTUREHANDLER_H_
#define CAPTUREHANDLER_H_

#include "avcap-export.h"

namespace avcap
{
	//! Abstract base class for capture handlers.

	/*! If an application wants to capture data, it must implement a CaptureHandler and must register it
	 * with the VidCapManager of the CaptureDevice. The VidCapManager will call handleCaptureEvent() 
	 * always a new frame has been captured. If the buffer isn't used  
	 * anymore the IOBuffer::release() method must be called in order to enable the
	 * VidCapManager to reuse or release the buffer. */

	class AVCAP_Export CaptureHandler
	{
	public:
		//! Consturctor
		inline CaptureHandler() 
			{}
			
		//! Destructor
		virtual inline ~CaptureHandler() 
			{}
		
		//! This method is called if a new frame has been captured by the VidCapManager.
		/*! If the buffer isn't used anymore, then the method IOBuffer::release() must be
		 * called. 
		 * \param io_buf The buffer containing the captured frame. */
		virtual void handleCaptureEvent(class IOBuffer* io_buf) = 0;
	};
}

#endif // CAPTUREHANDLER_H_
