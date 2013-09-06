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


#ifndef DSTUNER_H_
#define DSTUNER_H_

#include <string>

#include "Tuner_avcap.h"
#include "avcap-export.h"

class IAMTVAudio;
class IAMTVTuner;
class IBaseFilter;

namespace avcap
{
class DS_DeviceDescriptor;

	//! Implementation of the Tuner-class for DirectShow.
	  
	/*! This implementation uses the PROPSETID_TUNER property-set which is defined in 
	 * the library kstvtuner.ax which can not be linked with gcc. 
	 * It thus can not be used under mingw, but only if build with Visual-C++.
	 * So the mingw-build library misses this tuner-implentation.
	 * 
	 * There are only three states of signal strength:
	 * signal present, signal not present, signal not determined. Because of this limitation
	 * the method getAFCValue() is not implemented.
	 */
	
	class AVCAP_Export DS_Tuner : public Tuner
	{
	private:
	
#pragma warning( disable : 4800 )

		enum
		{
			TUNER_RADIO = 0x00080000, // CAP_RADIO --> see "HelpFunc.h" file
			TUNER_ANALOG_TV = 0x00020000 // CAP_TUNER --> see "HelpFunc.h" file
		};
	
		int 			mIndex;
		std::string 	mName;
		int 			mType;
		int 			mCapabilities;
		unsigned int 	mRangeHigh;
		unsigned int 	mRangeLow;
		double 			mStep;
		double 			mFreq;
	
	private:
		DS_DeviceDescriptor* mDSDeviceDescriptor;
	
	public:
		DS_Tuner(DS_DeviceDescriptor *dd, int index, const std::string &name,
				int type, int caps, unsigned int high, unsigned int low);
		
		virtual ~DS_Tuner();
	
		inline bool isRadioTuner() const
			{ return mType & TUNER_RADIO; }
	
		inline bool isTVTuner() const
			{ return mType & TUNER_ANALOG_TV; }
	
		int setStereo();
	
		int setMono();
	
		int setSAP();
	
		int setLang1();
	
		int setLang2();
	
		double getFreq() const;
	
		inline double getFreqStep() const
			{ return mStep; }
	
		inline double getMinFreq() const
			{ return mRangeLow*mStep; }
	
		inline double getMaxFreq() const
			{ return mRangeHigh*mStep; }
	
		const std::string getName() const
			{ return mName; }
	
		int getSignalStrength() const;
	
		int increaseFreq();
	
		int decreaseFreq();
	
		int setFreq(double f);
	
	private:
		int setAudioMode(int mode);
		
		int getAudioMode();
	
		bool getIAMTVTunerInterfaceFromFilter(IBaseFilter *Filter,
			IAMTVTuner **Tuner) const;

		bool getIAMTVAudioInterfaceFromFilter(IBaseFilter *Filter,
			IAMTVAudio **Audio);
	};
}

#endif // DSTUNER_H_
