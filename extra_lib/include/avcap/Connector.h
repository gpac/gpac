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


#ifndef CONNECTOR_H_
#define CONNECTOR_H_

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include "avcap-config.h"
#endif

#ifdef AVCAP_LINUX
# include <linux/types.h>
# ifdef AVCAP_HAVE_V4L2
#  include <linux/videodev2.h>
# else
#  include <linux/videodev.h>
# endif
#endif

#include <string>
#include <list>

#include "avcap-export.h"

namespace avcap
{	
	// forward declaration
	class DeviceDescriptor;
	class Tuner;
	
	//! This class is the abstraction of a video/audio input or output.
	
	/*! It is used to describe available inputs and outputs of a device and 
	 * to select them by means of the API-dependent CaptureDevices implementation of the ConnectorManager. 
	 * The ConnectorManager queries all available connectors of a device and 
	 * provides methods to set and get the currently used ones.
	 */
	 
	class AVCAP_Export Connector
	{
	public:
#ifdef AVCAP_LINUX
		enum
		{
			INPUT_TYPE_TUNER = V4L2_INPUT_TYPE_TUNER
		};
#endif
		
	protected:
		DeviceDescriptor*	mDeviceDescriptor;
		int					mIndex;
		int					mAudioset;
		int					mType;
		std::string			mName;
		
	public:
		//! The Constructor. Objects of this class are created by the ConnectorManager.
		inline Connector(DeviceDescriptor *dd, int index, const std::string& name, int type=0, int audioset=0):
			mDeviceDescriptor(dd), mIndex(index), mAudioset(audioset), mType(type), mName(name)
			{}

		//! The Destructor. 
		virtual inline ~Connector() 
			{}

		//! Returns the unique index of the connector.
		/*! \return The index. */
		inline int getIndex() const 
			{ return mIndex; }
		
		//! Get mapping of audio inputs to video inputs.
		/*! For devices which provide audio and video capturing, 
		 * video inputs can correspond to zero or more audio inputs. The audio inputs 
		 * are numbered from 0 to N-1, N <= 32. Each bit of the audioset corresponds
		 * to one input. For details, see the Video4Linux2 API Documentation.
		 * \n\n<b>Win32:</b> A video connector can correspond to only one audio connector
		 * (only one bit can be set at a time). */
		inline int getAudioset() const 
			{ return mAudioset; }
		
		//! Get the tuner associated with the Connector.
		/*! If a tuner is associated whith the connector (e.g. for TV-Tuner cards), then this method returns
		 * an object of class Tuner to access the tuner specific functionality.
		 * \return object of class Tuner or 0 if there is no tuner. */
		virtual inline Tuner* getTuner() 
			{ return 0; }
		
		//! Provides a textual description of the connector. 
		/*! \return connector name. */
		inline const std::string& getName() const 
			{ return mName; }
		
		//! Test, whether a tuner is associated with the connector or not.
		/*! \return true, if tuner is associated, false otherwise. */
		virtual inline bool hasTuner() const 
			{ return false; }
	};
};

#endif // CONNECTOR_H_
