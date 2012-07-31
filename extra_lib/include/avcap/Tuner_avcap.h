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

#ifndef TUNER_H_
#define TUNER_H_

#include <string>

#include "avcap-export.h"

namespace avcap
{
	class DeviceDescriptor;
	
	//! Interface of a tuner.
	
	/*! This class provides access to the tuner functionality of TV or Radio-cards.
	 * Applications can adjust things like frequency, audio mode etc. 
	 * Applications don't create Tuner objects themselfes but get them from the Connector
	 * the tuner is associated with. The connector in turn can be obtained from the 
	 * ConnectorManager of the CaptureDevice.
	 **/
	 
	class AVCAP_Export Tuner
	{
	private:
	public:
		virtual inline ~Tuner()
			{}
		
		//! Determine whether the tuner is able to receive radio frequencies. 
		/*! The default implementation returns false.
		 * \return true if radio tuner, false else */
		virtual inline bool isRadioTuner() const 
			{ return false; }

		//! Determine whether the tuner is able to receive TV frequencies.
		/*! The default implementation returns false.
		 * \return true if TV tuner, false else */
		virtual inline bool isTVTuner() const 
			{ return false; };

		//! Set the audio mode to stereo.
		/*! The default implementation is noop and returns -1.
		 * \return 0, if successful, -1 else. */		
		virtual inline int setStereo()  
			{ return -1; }

		//! Set the audio mode to mono.
		/*! Default implementation is noop and return -1.
		 * \return 0, if successful, -1 else. */		
		virtual inline int setMono() 
			{ return -1; }
		
		//! Set the audio mode to secondary audio program (SAP).
		/*! The default implementation is noop and returns -1.
		 * \return 0, if successful, -1 else. */		
		virtual inline int setSAP() 
			{ return -1; }
		
		//! Set the audio mode to language 1.
		/*! The default implementation is noop and returns -1.
		 * \return 0, if successful, -1 else. */		
		virtual inline int setLang1() 
			{ return -1; }

		//! Set the audio mode to language 2.
		/*! The default implementation is noop and returns -1.
		 * \return 0, if successful, -1 else. */		
		virtual inline int setLang2() 
			{ return -1; }

		//! Returns the current frequency in MHz.
		/*! The default implementation is noop and returns -1.
		 * \return tuner frequency */
		virtual inline double getFreq() const 
			{ return -1.0f; }

		//! Returns the step with in which the frequency can be increased or decreased.
		/*! The default implementation is noop and returns -1.
		 * \return frequency step width*/
		virtual inline double getFreqStep() const
			{ return -1.0f; }
		
		//! Returns the minimum possible frequency in MHz which can be applied to the tuner. 
		/*! The default implementation is noop and returns -1.
		 * \return minimal tuner frequency */
		virtual inline double getMinFreq() const
			{ return -1.0f; }

		//! Returns the maximum possible frequency in MHz which can be applied to the tuner. 
		/*! The default implementation is noop and returns -1.
		 * \return maximal tuner frequency */
		virtual inline double getMaxFreq() const
			{ return -1.0f; }
		
		//! Returns the tuner name. 
		/*! Default implementation returns an empty string.
		 * \return tuner name */
		virtual inline const std::string getName() const
			{ return ""; }
		
		//! This method tries to readjust and to fine-tune the frequency by means of the current AFC-value.
		/*! The default implementation is noop and returns -1.
		 * \return 0, if successful, -1 else. */
		virtual inline int finetune(int maxsteps) 
			{ return -1; }
		
		//! Get the current automatic frequency control (AFC) value.  
		/*! If the afc value is negative, the frequency is too low, if positive it is too high. 
		 *! The default implementation is noop and returns -1.
		 * \return afc */
		virtual inline int getAFCValue() const
			{ return -1; }
		
		//! Return the strength of the signal. 
		/*! The default implementation is noop and returns -1.
		 * \return signal strength */
		virtual inline int getSignalStrength() const
			{ return -1; }
		
		//! Increase the frequency a step corresponding to getFreqStep(). 
		/*! The default implementation is noop and returns -1.
		 * \return 0, if successful, -1 else. */
		virtual inline int increaseFreq() 
			{ return 0; }
		
		//! Decrease the frequency a step corresponding to getFreqStep(). 
		/*! The default implementation is noop and returns -1.
		 * \return 0, if successful, -1 else. */		
		virtual inline int decreaseFreq() 
			{ return 0; }
		
		//! Set the new frequency. The frequency is given in MHz.
		/*! The default implementation is noop and returns -1.
		 * \param f The new frequency.
		 * \return 0, if successful, -1 else. */
		virtual inline int setFreq(double f) 
			{ return -1; }
	};
	
	/*! Known european terrestric analog TV channels. They must be multiplied by 10^6 
	 * to obtain the frequency in Hz. */
	static const double TV_Channels[] = 
	{
		48.25, 55.25, 62.25, 175.25, 182.25, 189.25, 196.25, 203.25, 210.25, 217.25, 224.25, 471.25, 479.25, 487.25,
	 495.25, 503.25, 511.25, 519.25, 527.25, 535.25, 543.25, 551.25, 559.25, 567.25, 575.25, 583.25, 591.25,
	 599.25, 607.25, 615.25, 623.25, 631.25, 639.25, 647.25, 655.25, 663.25, 671.25, 679.25, 687.25, 695.25,
	 703.25, 711.25, 719.25, 727.25, 735.25, 743.25, 751.25, 759.25, 767.25, 775.25, 783.25, 791.25, 799.25,
	 807.25, 815.25, 823.25, 831.25, 839.25, 847.25, 855.25
	};

	/*! The number of TV channels */	
	static const int TV_Num_Channels = 60;

	/*! Some german analog radio channels. They must be multiplied by 10^6 
	 * to obtain the frequency in Hz. */
	static const double Radio_Channels[] = 
	{
		102.4, 102.0
	};

	/*! The number of radio channels */	
	static const int Radio_Num_Channels = 2;
	
}

#endif // TUNER_H_
