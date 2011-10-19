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


#ifndef CROSSBAR_H_
#define CROSSBAR_H_

#include <DShow.h>

#include "avcap-export.h"

namespace avcap
{
	struct AVCAP_Export STConnector
	{
		std::string NameOfConnector;
		LONG PhysicalType;
		bool IsAudioConnector;
		bool IsVideoConnector;
		bool IsRouted;
	
		IAMCrossbar *Crossbar;
		long PinIndexOfCrossbar;
		int AudioSet; 
		
		/* Video inputs correspond to zero or max. one audio input. 
		 The pins are numbered from 0 to N-1, N <= 32.
		 Each bit set is the index of a audio pin on the 
		 same crossbar corresponding to the selected video pin.
		 AudioSet only used for video input pins. */
	
		int PinIndex; // Independent PinIndex
	};
	
	struct AVCAP_Export STRouting
	{
		long OutputPinIndex;
		IPin *OutputPin;
		long RelatedInPinIndex;
		IPin *RelatedInputPin;
		IAMCrossbar *Crossbar;
		int CrossbarIndex; // Zero based index
		bool HasConnector;
	};
	
	//! A class for controlling DirectShow video crossbars.
	
	/*! This class creates a single object which encapsulates all connected
	 *	crossbars, enumerates all unique inputs which can be reached from
	 *  a given starting downstream filter.
	 *  
	 *  The class supports an arbitrarily complex graph of crossbars, 
	 *  which can be cascaded and disjoint, that is not all inputs need 
	 *  to traverse the same set of crossbars.
	 *  
	 *  Given a starting filter (typically the capture filter), the class 
	 *  recursively traces upstream searching for all viable inputs.  
	 *  An input is considered viable if it is either: 
	 * 
	 *  - unconnected
	 *  - connects to a DirectShow filter which does not support IAMCrossbar 
	 * COM-Interface.	
	 * */
	
	class AVCAP_Export CCrossbar
	{
	public:
		CCrossbar(ICaptureGraphBuilder2 *CaptureGraphBuilder);
		~CCrossbar(void);
	
		/*! Searches upstrean for all available crossbars in the filter graph, 
		 * starting from a given filter.
		 * \param StartFilter Filter to start search from.
		 * \return The number of crossbars found in the filter graph, -1 else. */
		int FindAllCrossbarsAndConnectors(IBaseFilter *StartFilter);
		std::list<STConnector*>& GetInputConnectorList();
	
		/*! Gets the currently selected video input connector.
		 * \param Connector Connector data */
		bool GetCurrentVideoInput(STConnector *Connector);
	
		/*! Gets the currently selected audio input connector.
		 * \param Connector Connector data */
		bool GetCurrentAudioInput(STConnector *Connector);
	
		/*! Sets the input connector.
		 * \param PinIndex Pin index */
		bool SetInput(int PinIndex);
	
	private:
		QzCComPtr<ICaptureGraphBuilder2> m_CaptureBuilder;
		std::list<IAMCrossbar*> m_CrossbarList;
		std::list<STRouting*> m_RoutingList;
		std::list<STConnector*> m_InputConnectorList;
	
		HRESULT GetCrossbarIPinAtIndex(IAMCrossbar *pXbar, LONG PinIndex,
				BOOL IsInputPin, IPin ** ppPin);
		void StringFromPinType(std::string &PinName, long lType);
		void DeleteAllLists();
	};
}

#endif // CROSSBAR_H_
