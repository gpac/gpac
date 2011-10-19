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


#ifndef V4L1_VIDCAPMANAGER_H_
#define V4L1_VIDCAPMANAGER_H_

#include <sys/types.h>
#include <list>
#include <time.h>

#include "CaptureManager.h"

namespace avcap
{
	class V4L1_DeviceDescriptor;
	class V4L1_FormatManager;
	class IOBuffer;
	class CaptureHandler;

	//! The Video4Linux2-API video capture manager.

	class V4L1_VidCapManager: public CaptureManager
	{
	private:
		typedef std::list<IOBuffer*> IOBufList_t;
		typedef std::list<int>		 IndexList_t;
				
		V4L1_DeviceDescriptor	*mDeviceDescriptor;
		V4L1_FormatManager		*mFormatMgr;
		
		IOBufList_t			mBuffers;
		IndexList_t			mCaptureIndices;
		
		int					mNumBufs;
		int					mMethod;
		int					mState;

		pthread_t*			mThread;
		int					mFinish;
		pthread_mutex_t		mLock;
		timeval				mStartTime;

		long 				mSequence;

		unsigned char*		mVideobuf;
		size_t				mVideobufSize;
		int					mWidth;
		int					mHeight;
		unsigned int		mPalette;
		int					mAvailableBuffers;
		
	public:
	
		V4L1_VidCapManager(V4L1_DeviceDescriptor* dd, V4L1_FormatManager* fmt_mgr, int nbufs = DEFAULT_BUFFERS);
		
		virtual ~V4L1_VidCapManager();
		
		int init();
		
		int destroy();
		
		int startCapture();

		int stopCapture();
		
		int getNumIOBuffers();
		
	private:
		int start_read();

		int start_mmap();

		int stop_read();
		
		int stop_mmap();

		IOBuffer* dequeue();
		
		int enqueue(IOBuffer* buf);
		
		IOBuffer* findBuffer(int index);
		
		static void run(void* mgr);
		
		void clearBuffers();
		
		int getUsedBufferCount();
	};
};
#endif // V4L1_VIDCAPMANAGER_H_
