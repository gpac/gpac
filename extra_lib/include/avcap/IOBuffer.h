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


#ifndef IOBUFFER_H_
#define IOBUFFER_H_


#include <sys/types.h>
#include <time.h>
#include <iostream>

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include <sys/time.h>
# include "avcap-config.h"
#endif

#ifdef _WIN32
# include <windows.h>
#endif

#include "CaptureManager.h"
#include "avcap-export.h"

namespace avcap
{
	//! The buffer to store captured data.
	
	/*! The class contains the captured data and provides additional information,
	 * e.g. sequence number, valid bytes and a capture timestamp. The data in the buffer
	 * may not correspond exactly to one frame, e.g. if the captured data is part 
	 * of a stream (e.g. MPEG).
	 * */
	 
	class AVCAP_Export IOBuffer
	{
	public:
		
		//! Use-state of the buffer
		enum State
		{
			STATE_USED = 0,		//!> currently used
			STATE_UNUSED,		//!> currently unused
		};
		
	private:
		CaptureManager *mMgr;
		void*			mPtr;
		size_t			mSize;
		int				mIndex;
		int				mState;
		long 			mSequence;
		size_t			mValid;
		struct timeval 	mTimestamp;
		
	public:
		
		//! Constructor
		IOBuffer(CaptureManager* mgr, void* ptr, size_t size, int index = 0);
		virtual ~IOBuffer();

		//! Get the pointer to the frame data.
		/*! \return the captured data. */
		inline void* getPtr() const
			{ return mPtr; }
		
		//! Returns the maximum number of bytes the buffer can contain.
		/*! \return Size of buffer in bytes. */
		inline size_t getSize() const
			{ return mSize; }
		
		//! Return the sequence number of the frame. 
		/*! \return Sequence number */
		inline long getSequence() const 
			{ return mSequence; }
		
		//! Returns the number of valid bytes in the buffer.
		/*! \return Number of valid bytes */
		inline size_t getValidBytes() const 
			{ return mValid; }
		
		//! Returns a timestamp in milliseconds. 
		/*! \return timestamp */
		unsigned long getTimestamp();

		//! Must be called by the application after the buffer isn't used anymore to to enable its reutilization.
		void release();
		
		//! Get the index of the buffer.
		/*! \return the buffer index. */
		inline int getIndex() const 
			{ return mIndex; } 

		//! Set the state of the buffer.
		/*! This method should not be used by applications.
		 *! \param state : the new state */
		inline void setState(State state) 
			{ mState = state; }

		//! Get the buffer usage state.
		/*! \return the current buffer state. */
		inline State getState() const 
			{ return (State) mState; }
		
		//! Set buffer parameters.
		/*! This method should not be used by applications. 
		 * \param valid : number of valid bytes in buffer
		 * \param state : the current buffer state
		 * \param ts : the timestamp the data was captured
		 * \param seq : the sequence number of the captured data */
		void setParams(const size_t valid, State state, struct timeval &ts, int seq);
	};
}

#endif // IOBUFFER_H_
