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


#ifdef HAS_AVC_SUPPORT

#ifndef AVC_VIDCAPMANAGER_H_
#define AVC_VIDCAPMANAGER_H_

#include "avcap-config.h"

#include <sys/types.h>
#include <list>
#include <time.h>

#include "CaptureManager.h"

namespace avcap
{
	class AVC_DeviceDescriptor;
	class AVC_FormatManager;
	class IOBuffer;
	class CaptureHandler;
	class AVC_Reader;
	class iec61883Connection;
	
	//! The IEEE 1394 AV/C video capture manager.
	
	/*! This class is used to capture video data from a AV/C-Device (e.g. DV-Cams) connected 
	 * to the computer via IEEE 1394 (aka Firewire or iLink) under Linux. 
	*/
	class AVC_VidCapManager: public CaptureManager
	{
	public:
		enum
		{
			MAX_BUFFERS = 32,	//!< The maximum number of IOBuffers.
			DEFAULT_BUFFERS = 8	//!< The default number of used IOBuffers.
		};
		
	private:
		typedef std::list<IOBuffer*> IOBufList;
		
		AVC_DeviceDescriptor	*mDeviceDescriptor;
		AVC_FormatManager		*mFormatMgr;
		AVC_Reader				*mReader;
		iec61883Connection		*mConnection;
		
		IOBufList			mBuffers;
		int					mNumBufs;
		int 				mSequence;

	public:
	
		AVC_VidCapManager(AVC_DeviceDescriptor* dd, AVC_FormatManager* fmt_mgr, int nbufs = DEFAULT_BUFFERS);
		
		virtual ~AVC_VidCapManager();
		
		int init();
		
		int destroy();
		
		int startCapture();

		int stopCapture();
		
		void registerCaptureHandler(CaptureHandler *handler); 

		void removeCaptureHandler(); 
		
		virtual int getNumIOBuffers();

	private:
		virtual IOBuffer* dequeue();

		virtual int enqueue(IOBuffer* buf);
	};
}
#endif // AVC_VIDCAPMANAGER_H_

#endif // HAS_AVC_SUPPORT

