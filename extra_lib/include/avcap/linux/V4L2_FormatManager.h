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

#ifndef V4L2_FORMATMANAGER_H_
#define V4L2_FORMATMANAGER_H_

#include "FormatManager.h"

namespace avcap
{
	class V4L2_DeviceDescriptor;
	
	//! This class implements the FormatManager for Video4Linux2 devices. */
	
	class V4L2_FormatManager: public FormatManager
	{
	
	public:
		V4L2_FormatManager(V4L2_DeviceDescriptor *dd);
		
		virtual ~V4L2_FormatManager();
		
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
		
		int setFramerate(int fps);
		
		int getFramerate();
		
		void query();

	private:
		int tryFormat();
		
		int getParams();
		
		void queryVideoStandards();
		
		void queryResolutions(Format* f);
	
	};
}

#endif // V4L2_FORMATMANAGER_H_
