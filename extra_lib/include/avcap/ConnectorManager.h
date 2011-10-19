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


#ifndef CONNECTORMANAGER_H_
#define CONNECTORMANAGER_H_

#include <list>

#include "Connector.h"
#include "Manager.h"
#include "avcap-export.h"

namespace avcap
{	
class DeviceCollector;

	//! This class is the interface to query and select the available video/audio inputs/outputs.
	 
	/*! This class manages STL-lists of objects of class Connector which describe 
	 * an input or output of a capture device. Applications can get these lists and get/set
 	 * the currently used connector of a special type. The methods to deal
	 * with outputs are only of partial interest for capturing but have been added
	 * for completeness.
	 * Application must use the API-dependent CaptureDevice-object to get an ConnectorManager.
	 * The default implementations of the mehtods in this class are a noop.
	 */

	class AVCAP_Export ConnectorManager: public Manager<Connector>
	{	
	protected:
		// Connector Lists
		ListType		mVideoInputs;
		ListType		mAudioInputs;
		ListType		mVideoOutputs;
		ListType		mAudioOutputs;
		
	public:		
		//! Construct the manager and query for available inputs and outputs for audio and video.
		/*! The manager is usualy created by an CaptureDevice object.
		 * \param dd The DeviceDescriptor to acces the device. */
		ConnectorManager(DeviceDescriptor *dd);

		//! The destructor. */
		virtual ~ConnectorManager() = 0;
		
		//! Returns the Connector describing the currently used video input.
		/*! The default-implementation returns 0. 
		 * \return video input connector.*/
		virtual inline Connector* getVideoInput()
			{ return 0; }

		//! Sets the currently used video input.
		/*! The default-implementation is a noop and returns -1. 
		 * \param c The connector to use for the video input.
		 * \return 0, if succesful, -1 else*/
		virtual inline int setVideoInput(Connector* c)
			{ return -1; }
			
		//! Returns the Connector describing the currently used audio input. 
		/*! The default-implementation returns 0. 
		 * \return audio input connector.*/
		virtual inline Connector* getAudioInput()
			{ return 0; }

		//! Sets the currently used audio input.
		/*! The default-implementation is a noop and returns -1. 
		 * \param c The connector to use for the audio input.
		 * \return 0, if succesful, -1 else*/
		virtual inline int setAudioInput(Connector* c)
			{ return -1; }
		
		//! Returns the Connector describing the currently used video output.
		/*! The default-implementation returns 0. 
		 * \return video output connector.*/
		virtual inline Connector* getVideoOutput()
			{ return 0; }
		
		//! Sets the currently used video output.
		/*! The default-implementation is a noop and returns -1. 
		 * \param c The connector to use for the video input.
		 * \return 0, if succesful, -1 else*/
		virtual inline int setVideoOutput(Connector* c)
			{ return -1; }
			
		//! Returns the Connector describing the currently used audio output. 
		/*! The default-implementation returns 0. 
		 * \return audio output connector.*/		
		virtual inline Connector* getAudioOutput()
			{ return 0; }

		//! Sets the currently used audio output.
		/*! The default-implementation is a noop and returns -1. 
		 * \param c The connector to use for the audio output.
		 * \return 0, if succesful, -1 else*/
		virtual inline int setAudioOutput(Connector* c)
			{ return -1; }

		//! Get the list of available video inputs of the device.
		/*! \return STL-list of pointers to objects of type 
		 * Connector describing the available video inputs. */
		inline const ListType& getVideoInputList() const 
			{ return mVideoInputs; }
		
		//! Get the list of available audio inputs of the device.
		/*! \return STL-list of pointers to objects of type 
		 * Connector describing the available audio inputs. */		
		inline const ListType& getAudioInputList() const 
			{ return mAudioInputs; }	

		//! Get the list of available video outputs of the device.
		/*! \return STL-list of pointers to objects of type 
		 * Connector describing the available video outputs. */
		inline const ListType& getVideoOutputList() const 
			{ return mVideoOutputs; }	

		//! Get the list of available audio outputs of the device.
		/*! \return STL-list of pointers to objects of type 
		 * Connector describing the available audio outputs. */
		inline const ListType& getAudioOutputList() const 
			{ return mAudioOutputs; }

		//! This method is called after creation to query for video/audio in- and outputs.
		virtual void query() = 0;

	private:
		void clearList(ConnectorManager::ListType& list);
	};
}

#endif //CONNECTORMANAGER_H_
