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


#ifndef V4L1_FORMATMANAGER_H_
#define V4L1_FORMATMANAGER_H_

#include <linux/videodev.h>

#include "FormatManager.h"

namespace avcap
{
	class V4L1_DeviceDescriptor;
	
	//! This class implements the FormatManager for Video4Linux2 devices. */
	/*! This CaptureManager starts an own thread to capture data. 
	 * The access to the internal buffer-list is synchronized, so \c release() can be called
	 * from any thread at any time.
	 */
		 
	class V4L1_FormatManager: public FormatManager
	{
	private:
		struct fmtdesc {
			char	description[32];
			__u16	palette;
			__u32	fourcc;
			float	sizefactor;
		};
		
		static const int mNumDescriptors = 18; 

		static const fmtdesc mDescriptors[mNumDescriptors];
		
		bool mIsOVFX2;
		
	public:
		V4L1_FormatManager(V4L1_DeviceDescriptor *dd);
		
		virtual ~V4L1_FormatManager();
		
		int setFormat(Format *fmt);
		
		int setFormat(unsigned int fourcc);

		Format* getFormat();
		
		int setResolution(int w, int h);
		
		int setBytesPerLine(int bpl);

		int getWidth();

		int getHeight();
		
		int getBytesPerLine();

		int flush();

		size_t getImageSize();

		const VideoStandard* getVideoStandard();

		int setVideoStandard(const VideoStandard* std);
		
		//! Setting the frame-rate is currently only possible for some philips-cams
		int setFramerate(int fps);
		
		int getFramerate();

		__u16 getPalette();
		
		void query();

	private:
		int getParams();

		void queryResolutions();
		
		__u16 getPalette(__u32 fourcc);
		
		int updateDimensions();
	};
}

#endif //V4L1_FORMATMANAGER_H_
