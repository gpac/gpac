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


#ifndef DSCONNECTORMANAGER_H_
#define DSCONNECTORMANAGER_H_

#include <list>

#include "Connector.h"
#include "ConnectorManager.h"
#include "avcap-export.h"

class IPin;
class IBaseFilter;

namespace avcap
{
class DS_DeviceDescriptor;
class DS_Connector;

	//! DirectShow implementation of the ConnectorManager.

	class AVCAP_Export DS_ConnectorManager : public ConnectorManager
	{
	private:
		void*					mCrossbar;
		DS_DeviceDescriptor* 	mDSDeviceDescriptor;
	
	public:
	
		/*! Construct the manager and query for available inputs and outputs for audio and video.
		 * The manager is usualy created by an CaptureDevice object.
		 * \param dd The DeviceDescriptor to acces the device. */
		DS_ConnectorManager(DS_DeviceDescriptor *dd);
	
		/*! The destructor. */
		virtual ~DS_ConnectorManager();
	
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
	
		int setConnector(bool IsVideoConnector, Connector* c);
		
		Connector* getConnector(bool IsVideoConnector);
		
		Connector* findByIndex(const ListType& l, int index);
	
		void getIPinFromConnector(Connector *con, IPin **ConnectorPin,
				IBaseFilter *CaptureFilter);
		
		bool removeAllFilters(IPin *OutputPinToStart);
		
		bool getPinCategory(IPin *Pin, GUID *PinCategory);
	};
}

#endif // DSCONNECTORMANAGER_H_
