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


#ifndef DSVIDCAPMANAGER_H_
#define DSVIDCAPMANAGER_H_

#include <list>

#include "CaptureManager.h"
#include "SampleGrabberCallback.h"
#include "avcap-export.h"

namespace avcap
{
class DS_DeviceDescriptor;
class DS_FormatManager;
class IOBuffer;
class CaptureHandler;
class SampleGrabberCallback;

	//! Implementation of the CaptureManager for DirectShow.

	class AVCAP_Export DS_VidCapManager : public CaptureManager
	{
	private:
		typedef std::list<IOBuffer*> IOBufList;
		
		DS_DeviceDescriptor*	mDSDeviceDescriptor;
		SampleGrabberCallback*	mGrabberCallback;
		HANDLE					mLock;
		int 					mSequence;
		int 					mNumBuffers;

	public:
		DS_VidCapManager(DS_DeviceDescriptor* dd, DS_FormatManager* fmt_mgr,
				int nbufs = DEFAULT_BUFFERS);
	
		virtual ~DS_VidCapManager();
	
		int init();
	
		int destroy();
	
		int startCapture();
	
		int stopCapture();
		
		int getNumIOBuffers();
	
	private:
		IOBuffer* dequeue();

		int enqueue(IOBuffer* buf);
	
		friend class IOBuffer;
		friend class SampleGrabberCallback;
	};

	class MutexGuard
	{
	private:
		HANDLE	mLock;
		
	public:
		inline MutexGuard(HANDLE lock) : 
			mLock(lock)
			{ WaitForSingleObject(mLock, INFINITE); }
			
		inline virtual ~MutexGuard()
			{ ReleaseMutex(mLock); }
	private:
		inline MutexGuard()
			{}
	};
}

#endif // DSVIDCAPMANAGER_H_
