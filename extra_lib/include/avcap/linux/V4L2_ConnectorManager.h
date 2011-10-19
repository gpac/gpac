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


#ifndef V4L2_CONNECTORMANAGER_H_
#define V4L2_CONNECTORMANAGER_H_

#include <list>

#include "ConnectorManager.h"

namespace avcap
{	
	class V4L2_DeviceDescriptor;
	
	//! This class implements the ConnectorManager for Video4Linux2-devices.
	
	class V4L2_ConnectorManager: public ConnectorManager
	{	
	public:
	
		V4L2_ConnectorManager(V4L2_DeviceDescriptor *dd);

		virtual ~V4L2_ConnectorManager();
		
		Connector* getVideoInput();

		int setVideoInput(Connector* c);
		
		Connector* getAudioInput();

		int setAudioInput(Connector* c);
		
		Connector* getVideoOutput();
		
		int setVideoOutput(Connector* c);
		
		Connector* getAudioOutput();
		
		int setAudioOutput(Connector* c);

		void query();
	
	private:
		Connector* findByIndex(const ListType& l, int index);
	};
}

#endif // V4L2_CONNECTORMANAGER_H_
