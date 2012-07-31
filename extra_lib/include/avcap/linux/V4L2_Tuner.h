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


#ifndef V4L2_TUNER_H_
#define V4L2_TUNER_H_

#include <linux/types.h>
#include <sys/types.h>
#include <string>

#include "Tuner_avcap.h"
#ifdef AVCAP_HAVE_V4L2
#include <linux/videodev2.h>
#else
#include <linux/videodev.h>
#endif

namespace avcap
{
	class V4L2_DeviceDescriptor;
	
	//! Implementation of the Tuner class for Video4Linux2 -devices.
	
	class V4L2_Tuner: public Tuner
	{
	private:

		enum {
			TUNER_RADIO = V4L2_TUNER_RADIO,
			TUNER_ANALOG_TV = V4L2_TUNER_ANALOG_TV,
		};
		
		enum {
			TUNER_CAP_LOW = V4L2_TUNER_CAP_LOW
		};

		typedef __u32	     uint;

	private:
		V4L2_DeviceDescriptor*	mDeviceDescriptor;
		int			mIndex;
		std::string	mName;
		int			mType;
		int			mCapabilities;
		uint 		mRangeHigh;
		uint 		mRangeLow;
		double		mStep;
		
	public:
		V4L2_Tuner(V4L2_DeviceDescriptor *dd, int index, const std::string &name, int type, int caps, uint high, uint low);
		virtual ~V4L2_Tuner();
		
		inline bool isRadioTuner() const 
			{ return mType & TUNER_RADIO; }

		inline bool isTVTuner() const 
			{ return mType & TUNER_ANALOG_TV; };

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
		
		inline const std::string getName() const
			{ return mName; }
		
		int finetune(int maxsteps);
		
		int getAFCValue() const;
		
		int getSignalStrength() const;
		
		int increaseFreq();
		
		int decreaseFreq();		
		
		int setFreq(double f);
		
	private:
		int setAudioMode(int mode);

		int getAudioMode();
	};
}

#endif // V4L2_TUNER_H_

