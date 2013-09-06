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


#ifndef DSCONNECTOR_H_
#define DSCONNECTOR_H_

#include <string>
#include <list>

#include "Connector.h"
#include "DeviceDescriptor.h"
#include "avcap-export.h"

namespace avcap
{
// forward declaration
class DS_DeviceDescriptor;
class DS_Tuner;

	//! Implementation of the Connector for DirectShow.


	class AVCAP_Export DS_Connector : public Connector
	{
	public:
	
#pragma warning( disable : 4800 )
		enum {
			INPUT_TYPE_TUNER = 0x00010000 // CAP_TUNERDEVICE --> see "HelpFunc.h" file
		};
	
	protected:
		DS_DeviceDescriptor* 	mDSDeviceDescriptor;
		int 					mIndex;
		int 					mAudioset;
		int						mType;
		std::string 			mName;
		DS_Tuner* 				mTuner;
	
	public:
		/*! The Constructor.  */
		DS_Connector(DS_DeviceDescriptor *dd, int index, const std::string& name,
				int type=0, int audioset=0, int tuner=0);
	
		/*! The Destructor. */
		virtual ~DS_Connector();
	
		bool hasTuner() const
			{ return mType & INPUT_TYPE_TUNER; }
	};
}

#endif // DSConnector_H_
