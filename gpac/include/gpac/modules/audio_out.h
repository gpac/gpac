/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / modules interfaces
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


/*

		Note on video driver: this is not a graphics driver, the only thing requested from this driver
	is accessing video memory and performing stretch of YUV and RGB on the backbuffer (bitmap node)
	the graphics driver is a different entity that performs 2D rasterization

*/

#ifndef _GF_MODULE_AUDIO_OUT_H_
#define _GF_MODULE_AUDIO_OUT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*include event system*/
#include <gpac/module.h>
/*include audio formats*/
#include <gpac/constants.h>


/*
	Audio hardware output module
*/

/*interface name and version for audio output*/
#define GF_AUDIO_OUTPUT_INTERFACE		GF_4CC('G','A','O', '6')

/*interface returned on query interface*/
typedef struct _audiooutput
{
	/* interface declaration*/
	GF_DECL_MODULE_INTERFACE

	/*setup system
		Win32: os_handle is HWND

	if num_buffer is set, the audio driver should work with num_buffers with a total amount of audio data
	equal to total_duration ms
	if not set the driver is free to decide what to do
	*/
	GF_Err (*Setup) (struct _audiooutput *aout, void *os_handle, u32 num_buffers, u32 total_duration);

	/*shutdown system */
	void (*Shutdown) (struct _audiooutput *aout);

	/*query output frequency available - if the requested sampleRate is not available, the driver shall return the best
	possible sampleRate able to handle NbChannels and NbBitsPerSample - if it doesn't handle the NbChannels
	the internal mixer will do it
	*/
	GF_Err (*QueryOutputSampleRate)(struct _audiooutput *aout, u32 *io_desired_samplerate, u32 *io_NbChannels, u32 *io_AudioFormat);

	/*set output config - if audio is not running, driver must start it
	*SampleRate, *NbChannels, *audioFormat:
		input: desired value
		output: final values
	channel_layout is the channels output cfg, eg set of flags as specified in constants.h
	*/
	GF_Err (*Configure) (struct _audiooutput *aout, u32 *SampleRate, u32 *NbChannels, u32 *audioFormat, u64 channel_layout);

	/*returns total buffer size used in ms. This is needed to compute the min size of audio decoders output*/
	u32 (*GetTotalBufferTime)(struct _audiooutput *aout);

	/*returns audio delay in ms, eg time delay until written audio data is outputed by the sound card
	This function is only called after ConfigureOutput*/
	u32 (*GetAudioDelay)(struct _audiooutput *aout);

	/*set output volume(between 0 and 100) */
	void (*SetVolume) (struct _audiooutput *aout, u32 Volume);
	/*set balance (between 0 and 100, 0=full left, 100=full right)*/
	void (*SetPan) (struct _audiooutput *aout, u32 pan);
	/*freezes soundcard flow - must not be NULL for self threaded
		PlayType: 0: pause, 1: resume, 2: reset HW buffer and play.
	*/
	void (*Play) (struct _audiooutput *aout, u32 PlayType);
	/*specifies whether the driver relies on the app to feed data or runs standalone*/
	Bool SelfThreaded;

	/*if not using private thread, this should perform sleep & fill of HW buffer
		the audio render loop in this case is: while (run) {driver->WriteAudio(); if (reconf) Reconfig();}
	the driver must therefore give back the hand to the renderer as often as possible - the usual way is:
		gf_sleep until hw data can be written
		write HW data
		return
	*/
	void (*WriteAudio)(struct _audiooutput *aout);

	/*if using private thread the following MUST be provided*/
	void (*SetPriority)(struct _audiooutput *aout, u32 priority);

	/*your private data handler - should be allocated when creating the interface object*/
	void *opaque;

	/*these are assigned by the audio renderer once module is loaded*/

	/*fills the buffer with audio data, returns effective bytes written - the rest is filled with 0*/
	u32 (*FillBuffer) (void *audio_renderer, u8 *buffer, u32 buffer_size);
	void *audio_renderer;

} GF_AudioOutput;


#ifdef __cplusplus
}
#endif


#endif	/*_GF_MODULE_AUDIO_OUT_H_*/

