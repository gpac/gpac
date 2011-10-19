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

#ifndef SAMPLEGRABBER_H_
#define SAMPLEGRABBER_H_

#include "QEdit.h"

#include "IOBuffer.h"
#include "avcap-export.h"

namespace avcap
{
class DS_VidCapManager;

	//! Data capture handler for DirectShow devices.
	
	/*! Each time new data arrives, the method
	 * SampleCB() is called and delivers the data to the video capture 
	 * manager set by the SetVideoCaptureManager() method.*/

	class AVCAP_Export SampleGrabberCallback : public ISampleGrabberCB
	{
	private:
		DS_VidCapManager* 	mVidCapMngr;
		ISampleGrabber* 	mSampleGrabberFilter;
		HANDLE&				mLock;

	public:
		SampleGrabberCallback(HANDLE& lock);
		
		~SampleGrabberCallback();
	
		/*! Sets the video capture manager. 
		 * New data will be delivered to the video capture manager.
		 * \param vidCapManager The video capture manager. */
		void SetVideoCaptureManager(DS_VidCapManager *vidCapManager);
	
		/*! Sets the samplegrabber filter. 
		 * Needed to get some information (e.g. currently used format).
		 * \param SampleGrabberFilter The samplegrabber filter. */
		void SetSampleGrabberFilter(ISampleGrabber *SampleGrabberFilter);
	
		/* Fake referance counting - This is safe because the application 
		 * creates the object on the stack, and the object remains in scope 
		 * throughout the lifetime of the filter graph.*/
		STDMETHODIMP_(ULONG) AddRef() 
			{ return 1; }
		
		STDMETHODIMP_(ULONG) Release() 
			{ return 2; }

		STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);
	
		/*! New data (captured data) arrives here and will be
		 * delivered to the video capture manager set 
		 * by the SetVideoCaptureManager() method. */
		STDMETHODIMP SampleCB(double Time, IMediaSample *pSample);

		STDMETHODIMP BufferCB(double Time, BYTE *pBuffer, long BufferLen);
	
		friend class IOBuffer;
	};
}

#endif // SAMPLEGRABBER_H_
