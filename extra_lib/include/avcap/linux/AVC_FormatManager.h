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

#ifndef AVC_FORMATMANAGER_H_
#define AVC_FORMATMANAGER_H_

#include "FormatManager.h"

namespace avcap
{
	class AVC_DeviceDescriptor;
	
	//! Implementation of the FormatManager for AV/C devices. 
	  
	/*! DV-Cams usually provide only a fixed resolution (PAL: 720x576, NTSC: 720x480)
	 * YUV-format and no choice of video standards.
	 * Additionally, this manager provides RGB.*/
	 
	class AVC_FormatManager: public FormatManager
	{
	private:
		bool mIsPal;
		
	public:
		AVC_FormatManager(AVC_DeviceDescriptor *dd);
		
		virtual ~AVC_FormatManager();
		
		//! Set the image with. 
		/*! For AV/C -devices the reolution is fixed and thus can't be realy modified.
		 * \param w: width
		 * \param h: height
		 * \return 0, if the desired resolution matches the native resolution, -1 else*/
		int setResolution(int w, int h);

		//! Get the current framerate. Setting the framerate is not possible.
		/*! \return 25 for PAL, 30 for NTSC(this is a little bit inaccurate, since the proper NTSC-frame rate is 29.97 fps)*/
		inline int getFramerate() { return mIsPal ? 25 : 30; }
		
		void query();
	};
};
#endif // AVC_FORMATMANAGER_H_

#endif // HAS_AVC_SUPPORT

