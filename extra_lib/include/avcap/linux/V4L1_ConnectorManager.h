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


#ifndef V4L1_CONNECTORMANAGER_H_
#define V4L1_CONNECTORMANAGER_H_

#include <list>

#include "ConnectorManager.h"

#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

namespace avcap
{	
	class V4L1_DeviceDescriptor;
	
	//! Implementation of the ConnectorManager for Video4Linux2-devices. */
	
	class V4L1_ConnectorManager: public ConnectorManager
	{	
	private:
		Connector*	mCurrentVideoInput;
		
	public:
		V4L1_ConnectorManager(V4L1_DeviceDescriptor *dd);
		
		virtual ~V4L1_ConnectorManager();
		
		Connector* getVideoInput();

		int setVideoInput(Connector* c);
		
		void query();
	
	private:
		Connector* findByIndex(const ListType& l, int index);
	};
}

#endif // V4L1_CONNECTORMANAGER_H_
