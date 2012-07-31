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


#ifndef V4L1_CONNECTOR_H_
#define V4L1_CONNECTOR_H_

#include <linux/types.h>

#include <string>
#include <list>

#include "Connector.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

namespace avcap
{	
	// forward declaration
	class V4L1_DeviceDescriptor;
	class Tuner;
	
	//! Implementation of the Connector for a Video4Linux1 device. 

	class V4L1_Connector : public Connector
	{
	public:
		//! The Constructor. Objects should be instantiated only by the ConnectorManager.
		inline V4L1_Connector(V4L1_DeviceDescriptor *dd, int index, const std::string& name, int type=0):
			Connector(dd, index, name, type)
			{}
		
		virtual inline ~V4L1_Connector()
			{}			
	};
}

#endif // V4L1_CONNECTOR_H_
