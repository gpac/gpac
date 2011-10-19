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


#ifndef V4L2_VIDCAPMANAGER_H_
#define V4L2_VIDCAPMANAGER_H_

#include <sys/types.h>
#include <list>
#include <time.h>

#include "CaptureManager.h"

namespace avcap
{
	class V4L2_DeviceDescriptor;
	class FormatManager;
	class IOBuffer;
	class CaptureHandler;

	//! The Video4Linux2-API video capture manager.
	
	/*! This class can be used to capture video data from a V4L2 video device. 
	 * The manager creates a defined number 
	 * of IOBuffers and permanently reuses them to store the captured data. 
	 * Since the number of these buffers is finite, it is important that applications 
	 * call IOBuffer::release() as soon as they don't need the data anymore. The
	 * access to the internal buffer-list is synchronized, so \c release() can be called
	 * from any thread at any time.
	 * Typical applications don't create objects of this class directly. They obtain
	 * an instance from CaptureDevice. */
	 
	class V4L2_VidCapManager: public CaptureManager
	{
	public:
		enum
		{
			MAX_BUFFERS = 32,	//!< The maximum number of IOBuffers.
			DEFAULT_BUFFERS = 16	//!< The default number of used IOBuffers.
		};
		
		enum IOMethod
		{
			IO_METHOD_NOCAP = 0,
	        IO_METHOD_READ,
	        IO_METHOD_MMAP,
	        IO_METHOD_USERPTR,
		};

	private:
		typedef std::list<IOBuffer*> IOBufList;
		
		V4L2_DeviceDescriptor	*mDeviceDescriptor;
		FormatManager		*mFormatMgr;

		IOBufList			mBuffers;
		int					mNumBufs;
		int					mMethod;
		int					mState;
		pthread_t*			mThread;
		int 				mFinish;
		pthread_mutex_t		mLock;
		timeval				mStartTime;

		int 				mSequence;
		int					mDTNumerator;
		int					mDTDenominator;
		int					mAvailableBuffers;
	
	public:
		V4L2_VidCapManager(V4L2_DeviceDescriptor* dd, FormatManager* fmt_mgr, int nbufs = DEFAULT_BUFFERS);
		
		virtual ~V4L2_VidCapManager();
		
		int init();
		
		int destroy();
		
		int startCapture();

		int stopCapture();
		
		int getNumIOBuffers();

	private:
		int start_read();
		int start_mmap();
		int start_userptr();

		int stop_read();
		int stop_mmap();
		int stop_userptr();

		IOBuffer* dequeue();
		int enqueue(IOBuffer* buf);
		
		IOBuffer* findBuffer(int index);
		static void run(void* mgr);
		
		void clearBuffers();
		int getUsedBufferCount();
	};
}

#endif // V4L2_VIDCAPMANAGER_H_
