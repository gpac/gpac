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

#ifndef QT_FORMATMANAGER_H
#define QT_FORMATMANAGER_H

#include <stdint.h>
#include <QuickTime/QuickTime.h>

#include "FormatManager.h"

namespace avcap
{
	class QT_DeviceDescriptor;
	
	//! Implementation of the FormatManager for the QuickTime-implementation of CaptureDevice.
	
	/*! This FormatManager supports at most one format, so setting the format is not realy neccessary. */
	
	class QT_FormatManager: public FormatManager
	{
	private:
		QT_DeviceDescriptor*	mQTDeviceDescriptor;
		GWorldPtr				mGWorld;
		Rect					mCurrentBounds;
		bool					mNeedsDecompression;
		
	public:
		QT_FormatManager(QT_DeviceDescriptor *dd);
		
		virtual ~QT_FormatManager();
		
		int setFormat(Format *fmt);
		
		int setFormat(uint32_t fourcc);

		Format* getFormat();
		
		int setResolution(int w, int h);

		int getWidth();

		int getHeight();
		
		size_t getImageSize();
				
		int setFramerate(int fps);
		
		int getFramerate();
		
		void query();
		
	private:
		
		int probeResolutions(Format* fmt);

		int checkResolution(int w, int h);

		int updateGWorld(const Rect* bounds);

		static uint32_t OSType2Fourcc(OSType pixel_format);
		
		static OSType Fourcc2OSType(uint32_t fourcc);

		inline bool needsDecompression() const 
			{ return mNeedsDecompression; }

		friend class QT_VidCapManager;
	};
}

#endif // QT_FORMATMANAGER_H
