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

#ifndef AVC_READER_H_
#define AVC_READER_H_

#include <list>

#include "ieee1394io.h"

namespace avcap
{
class AVC_VidCapManager;
class AVC_FormatManager;
class IOBuffer;

	//! AVC_Reader, used by the AVC_VidCapManager.
	
	class AVC_Reader : public iec61883Reader
	{
		typedef std::list<IOBuffer*>	BufferList_t;
		
		
		AVC_VidCapManager* mVidCapMgr;
		AVC_FormatManager*	mFormatMgr;
		CaptureHandler*		mCaptureHandler;
	
		BufferList_t	mBuffers;
		long			mSequence;
		int				mAvailableBuffers;
		
	public:
		AVC_Reader(AVC_VidCapManager* cap_mgr, AVC_FormatManager* fmt_mgr, int port, int channel, int num_bufs);
		
		virtual ~AVC_Reader();
	
		void registerCaptureHandler(CaptureHandler *handler);
	
		void removeCaptureHandler();
		
		virtual void TriggerAction();
		
		void enqueue(IOBuffer* io_buf);
		
		inline int getNumIOBuffers() 
			{ return mAvailableBuffers; }
	
	};	
}

#endif // AVC_READER_H_
#endif
