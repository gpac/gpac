/*
* ieee1394io.cc -- asynchronously grabbing DV data
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

#ifndef _IEEE1394IO_H
#define _IEEE1394IO_H 1

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libiec61883/iec61883.h>


#include <string>
using std::string;
#include <deque>
using std::deque;

namespace avcap
{
	
class Frame;

class IEEE1394Reader
{
protected:
	/// the number of frames that had to be thrown away because
	/// our inFrames queue did not contain available frames
	int droppedFrames;

	/// the number of frames that are tainted because they are incomplete -
	/// a packet was dropped
	int incompleteFrames;

	/// a pointer to the frame which is currently been transmitted
	Frame	*currentFrame;

	/// a list of empty frames
	deque < Frame* > inFrames;

	/// a list of already received frames
	deque < Frame* > outFrames;

public:

	IEEE1394Reader( int channel = 63, int frames = 5 );
	virtual ~IEEE1394Reader();

	// Mutex protected public methods
	virtual bool StartThread( void ) = 0;
	virtual void StopThread( void ) = 0;
	Frame* GetFrame( void );
	
	void DoneWithFrame( Frame* );
	int GetDroppedFrames( void );
	int GetIncompleteFrames( void );
	int GetOutQueueSize( void )
	{
		return outFrames.size();
	}
	int GetInQueueSize( void )
	{
		return inFrames.size();
	}

	// These two public methods are not mutex protected
	virtual bool Open( void ) = 0;
	virtual void Close( void ) = 0;

	bool WaitForAction( int seconds = 0 );
	virtual void TriggerAction( );

	virtual bool StartReceive( void ) = 0;
	virtual void StopReceive( void ) = 0;

protected:
	/// the iso channel we listen to (typically == 63)
	int	channel;

	/// contains information about our thread after calling StartThread
	pthread_t thread;

	/// this mutex protects capture related variables that could possibly
	/// accessed from two threads at the same time
	pthread_mutex_t mutex;

	// This condition and mutex are used to indicate when new frames are
	// received
	pthread_mutex_t condition_mutex;
	pthread_cond_t condition;

	/// A state variable for starting and stopping thread
	bool isRunning;

	void Flush( void );
};


typedef void (*BusResetHandler)( void* );
typedef void* BusResetHandlerData;

class CaptureHandler;

class iec61883Reader: public IEEE1394Reader
{
private:

	/// the interface card to use (typically == 0)
	int	m_port;

	/// the handle to libraw1394
	raw1394handle_t m_handle;

	/// the handle to libiec61883
	iec61883_dv_fb_t m_iec61883dv;

	BusResetHandler m_resetHandler;
	const void* m_resetHandlerData;

	CaptureHandler* m_captureHandler;
	
public:
	iec61883Reader( int port = 0, int channel = 63, int buffers = 5, 
		BusResetHandler = 0, BusResetHandlerData = 0 );
	~iec61883Reader();

	bool Open( void );
	void Close( void );
	bool StartReceive( void );
	void StopReceive( void );
	bool StartThread( void );
	void StopThread( void );
	int Handler( int length, int complete, unsigned char *data );
	void *Thread();
	void ResetHandler( void );
		

private:
	static int ResetHandlerProxy( raw1394handle_t handle, unsigned int generation );
	static int HandlerProxy( unsigned char *data, int length, int complete, 
		void *callback_data );
	static void* ThreadProxy( void *arg );
};


class iec61883Connection
{
private:
	raw1394handle_t m_handle;
	nodeid_t m_node;
	int m_channel;
	int m_bandwidth;
	int m_outputPort;
	int m_inputPort;

public:

	iec61883Connection( int port, int node );
	~iec61883Connection();

	static void CheckConsistency( int port, int node );
	int GetChannel( void ) const
	{
		return m_channel;
	}
	int Reconnect( void );
};

}

#endif
#endif
