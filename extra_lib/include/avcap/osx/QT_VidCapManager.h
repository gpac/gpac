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

#ifndef QT_VIDCAPMANAGER_H_
#define QT_VIDCAPMANAGER_H_

#include <sys/types.h>
#include <list>
#include <time.h>
#include <pthread.h>

#include <QuickTime/QuickTime.h>

#include "CaptureManager.h"

namespace avcap
{
	class QT_DeviceDescriptor;
	class QT_FormatManager;
	class IOBuffer;
	class CaptureHandler;
	
	//! Implementation of the CaptureManager for the QuickTime-implementation of a CaptureDevice.

	/*! This CaptureManager captures the data in an ow thread. 
	 * Decompression is performed, if neccessary (e.g. for DV-Cams). */
	
	class QT_VidCapManager: public CaptureManager
	{
	public:

	private:		
		typedef std::list<IOBuffer*> IOBufList_t;

		QT_DeviceDescriptor	*mQTDeviceDescriptor;
		QT_FormatManager	*mFormatMgr;
		IOBufList_t			mBuffers;
		TimeScale			mTimeScale;
		TimeScale			mLastTime;
		pthread_t*			mThread;
		pthread_mutex_t		mLock;
		int					mFinish;
		long 				mSequence;
		int					mNumBufs;
		int					mAvailableBuffers;
		ICMDecompressionSessionRef	mDecompSession;

	public:
		QT_VidCapManager(QT_DeviceDescriptor* dd, QT_FormatManager* fmt_mgr, int nbufs);
		
		virtual ~QT_VidCapManager();
		
		int init();
		
		int destroy();
		
		int startCapture();

		int stopCapture();
		
		virtual IOBuffer* dequeue();

		virtual int enqueue(IOBuffer* buf);
		
		virtual int getNumIOBuffers();
		
	private:
		int startCaptureImpl();
		
		int stopCaptureImpl();

		void notifyCaptureHandler(void* data, size_t length, size_t bytes_per_row, TimeValue time);

		int poll();

		struct timespec getPollTimespec(double fps);

		int decompressData(void* data, long length, TimeValue timeValue);

		static OSErr captureCallback(SGChannel ch, Ptr data, long data_len, long * offset, 
			long ch_ref_con, TimeValue time_value, short write_type, long priv);

		OSErr captureCallbackImpl(SGChannel ch, Ptr data, long data_len, long * offset, 
			long ch_ref_con, TimeValue time_value, short write_type);

		int createDecompSession();
		
		static void* threadFunc(void* arg);
	
		static void decompTrackingCallback(void *decompressionTrackingRefCon, OSStatus result,
			ICMDecompressionTrackingFlags decompressionTrackingFlags, CVPixelBufferRef pixelBuffer,
			TimeValue64 displayTime, TimeValue64 displayDuration, ICMValidTimeFlags validTimeFlags,
			void *reserved,	void *sourceFrameRefCon );

		void threadFuncImpl();
		
		void clearBuffers();
	};
}

#endif // QT_VIDCAPMANAGER_H

