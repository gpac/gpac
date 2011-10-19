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

#ifndef CAPTUREMANAGER_H_
#define CAPTUREMANAGER_H_

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include "avcap-config.h"
#endif

#include "avcap-export.h"

namespace avcap
{
	class CaptureHandler;
	class IOBuffer;
	
	//! Abstract interface to access capture related tasks of a CaptureDevice.
	
	/*! An implementation of this class is provided by the API-specific CaptureDevice. 
	 * The CaptureManager can be used by applications to register a CaptureHandler and
	 * to start/stop the capture. There is only one CaptureHandler at the same time.
	 * To distribute the captured data to more than one interested sink is the responsibility 
	 * of the application. */
	
	class AVCAP_Export CaptureManager
	{
	public:
		enum
		{
			MAX_BUFFERS = 32,	//!< The maximum number of IOBuffers.
			DEFAULT_BUFFERS = 16	//!< The default number of used IOBuffers.
		};
		
			
#ifdef AVCAP_LINUX
		enum IOMethod
		{
			IO_METHOD_NOCAP = 0,
	        IO_METHOD_READ,
	        IO_METHOD_MMAP,
	        IO_METHOD_USERPTR,
		};
#endif

	private:
		CaptureHandler*	mCaptureHandler;
		
	public:
		//! Constructor
		inline CaptureManager() : mCaptureHandler(0)
			{}
		
		//! Destructor
		virtual inline ~CaptureManager()
			{}
		
		//! Do basic initialization after startup.
		virtual int init() = 0;
		
		//! Called before object destruction.
		virtual int destroy() = 0;
		
		//! Start capturing data.
		virtual int startCapture() = 0;
		
		//! Stop capturing data.
		virtual int stopCapture() = 0;
		
		//! Register a capture handler.
		/*! Only one capture handler can be registered at the same time.
		 * The handlers CaptureHandler::handleCaptureEvent() method will be called,
		 * if new data has been captured. The ownership of the handler remains at the caller.
		 * He is responsible for removing and deleting the handler. 
		 * \param handler The capture handler implementation.*/
		virtual inline void registerCaptureHandler(CaptureHandler *handler) 
			{ mCaptureHandler = handler; }

		//! Remove the current capture handler.
		/*! If a capture handler was registered before, then this handler will not be 
		 * notified anymore if data has been captured. */ 
		virtual inline void removeCaptureHandler()
			{ mCaptureHandler = 0; }
		
		//! Get the current CaptureHandler.
		/*! Return the capture handler currently registered with registerCaptureHandler()
		 * or 0, if no handler was registered before.
		 * \return pointer to the capture handler */ 
		virtual inline CaptureHandler* getCaptureHandler()
			{ return mCaptureHandler; }
		
		//! Returns the number of IOBuffers currently available.
		/*! The CaptureManager usually waits to capture the next frame until an IOBuffer is available.
		 * The application is reponsible to release the IOBuffers to make it available to the capture manager.
		 * \return the number of IOBuffers. */
		virtual int getNumIOBuffers() = 0;
		
	private:
		//! Dequeue the next buffer.
		/*! \return the next buffer with captured data. */
		virtual IOBuffer* dequeue() = 0;

		//! Enqueue a buffer.
		/*! \param buf the buffer that isn't used by the application anymore and that can be reused now. 
		 * \return 0 success, -1 on failure*/
		virtual int enqueue(IOBuffer* buf) = 0;

		friend class IOBuffer;
	};
};

#endif // CAPTUREMANAGER_H_
