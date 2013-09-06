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

#ifndef DSFORMATMANAGER_H_
#define DSFORMATMANAGER_H_

#include <list>
#include <string>
#include <Dshow.h>

#ifndef _MSC_VER
# include <stdint.h>
#endif

#include "FormatManager.h"
#include "avcap-export.h"

class IBaseFilter;
class IPin;

namespace avcap
{
class DS_DeviceDescriptor;

	//! Implementation of the FormatManager for DirectShow.
	
	/*! A capture device that still uses a VFW (Video for Windows)
	* driver can select the available formats only by using the VFW driver-supplied dialog box.
	* But GUI-related stuff is out of the scope of this library. */
	
	class AVCAP_Export DS_FormatManager : public FormatManager
	{
	private:
		DS_DeviceDescriptor* mDSDeviceDescriptor;
		int			   mFrameRate;

	public:
		DS_FormatManager(DS_DeviceDescriptor *dd);

		virtual ~DS_FormatManager();
	
		virtual int setFormat(Format *fmt);
	
		virtual Format* getFormat();
	
		virtual int setResolution(int w, int h);
	
		virtual int getWidth();
	
		virtual int getHeight();
	
		virtual int getBytesPerLine();
	
		virtual int setFramerate(int fps);

		virtual int getFramerate();

		virtual int flush();
	
		virtual size_t getImageSize();
	
		virtual const VideoStandard* getVideoStandard();
	
		virtual int setVideoStandard(const VideoStandard* std);
	
		virtual void query();
	
	private:
		int getParams();
		
		void queryVideoStandards();
	
		bool getVideoInfoHeader(AM_MEDIA_TYPE *MediaType,
				VIDEOINFOHEADER *VideoInfoHeader);
		
		bool getCurrentConnectedVideoPin(IBaseFilter *CaptureFilter,
				IPin **VideoPin);
	};
}

#endif // DSFORMATMANAGER_H_
