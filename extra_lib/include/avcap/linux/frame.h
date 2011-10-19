/*
* frame.h -- utilities for process digital video frames
* Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef HAS_AVC_SUPPORT
#define HAVE_LIBDV

#ifndef _FRAME_H
#define _FRAME_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <string>
using std::string;

#include <libdv/dv.h>
#include <libdv/dv_types.h>
#define DV_AUDIO_MAX_SAMPLES 1944

#define FRAME_MAX_WIDTH 720
#define FRAME_MAX_HEIGHT 576

namespace avcap
{
	
typedef struct Pack
{
	/// the five bytes of a packet
	unsigned char data[ 5 ];
}
Pack;

typedef struct TimeCode
{
	int hour;
	int min;
	int sec;
	int frame;
}
TimeCode;


typedef struct AudioInfo
{
	int frames;
	int frequency;
	int samples;
	int channels;
	int quantization;
}
AudioInfo;


class VideoInfo
{
public:
	int width;
	int height;
	bool isPAL;
	TimeCode timeCode;
	struct tm	recDate;

	VideoInfo();

	//    string GetTimeCodeString();
	//    string GetRecDateString();
}
;


class Frame
{
public:
	/// enough space to hold a PAL frame
	unsigned char data[ 144000 ];
	/// the number of bytes written to the frame
	int bytesInFrame;
#ifdef HAVE_LIBDV

	dv_decoder_t *decoder;
#endif

	int16_t *audio_buffers[ 4 ];

public:
	Frame();
	~Frame();

	bool GetSSYBPack( int packNum, Pack &pack ) const;
	bool GetVAUXPack( int packNum, Pack &pack ) const;
	bool GetAAUXPack( int packNum, Pack &pack ) const;
	bool GetTimeCode( TimeCode &timeCode ) const;
	bool GetRecordingDate( struct tm &recDate ) const;
	string GetRecordingDate( void ) const;
	bool GetAudioInfo( AudioInfo &info ) const;
	bool GetVideoInfo( VideoInfo &info ) const;
	int GetFrameSize( void ) const;
	float GetFrameRate() const;
	bool IsPAL( void ) const;
	bool IsNewRecording( void ) const;
	bool IsNormalSpeed() const;
	bool IsComplete( void ) const;
	int ExtractAudio( void *sound ) const;
	void ExtractHeader( void );

#ifdef HAVE_LIBDV

	void SetPreferredQuality( );
	int ExtractAudio( int16_t **channels ) const;
	int ExtractRGB( void *rgb );
	int ExtractPreviewRGB( void *rgb );
	int ExtractYUV( void *yuv );
	int ExtractPreviewYUV( void *yuv );
	bool IsWide( void ) const;
	int GetWidth();
	int GetHeight();
	void SetRecordingDate( time_t *datetime, int frame );
	void SetTimeCode( int frame );
#endif

	void Deinterlace( void *image, int bpp );

private:
#ifndef HAVE_LIBDV
	/// flag for initializing the lookup maps once at startup
	static bool maps_initialized;
	/// lookup tables for collecting the shuffled audio data
	static int palmap_ch1[ 2000 ];
	static int palmap_ch2[ 2000 ];
	static int palmap_2ch1[ 2000 ];
	static int palmap_2ch2[ 2000 ];
	static int ntscmap_ch1[ 2000 ];
	static int ntscmap_ch2[ 2000 ];
	static int ntscmap_2ch1[ 2000 ];
	static int ntscmap_2ch2[ 2000 ];
	static short compmap[ 4096 ];
#endif
};

}

#endif
#endif
