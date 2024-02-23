/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / ROUTE (ATSC3, DVB-I) demuxer
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

#ifndef _GF_ROUTE_H_
#define _GF_ROUTE_H_

#include <gpac/tools.h>

#ifndef GPAC_DISABLE_ROUTE

#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/route.h>
\brief Specific extensions for ROUTE (ATSC3, DVB-I) protocol
*/

/*!
\addtogroup route_grp ROUTE
\ingroup media_grp
\brief ROUTE ATSC 3.0 receiver

The ROUTE receiver implements part of the ATSC 3.0 specification, mostly low-level signaling and ROUTE reception.
It gathers objects from a ROUTE session and sends them back to the user through a callback, or deletes them if no callback is sent.
The route demuxer does not try to repairing files, it is the user responsibility to do so.

@{
*/


/*! ATSC3.0 bootstrap address for LLS*/
#define GF_ATSC_MCAST_ADDR	"224.0.23.60"
/*! ATSC3.0 bootstrap port  for LLS*/
#define GF_ATSC_MCAST_PORT	4937

/*!The GF_ROUTEDmx object.*/
typedef struct __gf_routedmx GF_ROUTEDmx;

/*!The types of events used to communicate withe the demuxer user.*/
typedef enum
{
	/*! A new service detected, service ID is in evt_param, no file info*/
	GF_ROUTE_EVT_SERVICE_FOUND = 0,
	/*! Service scan completed, no evt_param, no file info*/
	GF_ROUTE_EVT_SERVICE_SCAN,
	/*! New MPD available for service, service ID is in evt_param, no file info*/
	GF_ROUTE_EVT_MPD,
	/*! static file update (with predefined TOI), service ID is in evt_param*/
	GF_ROUTE_EVT_FILE,
	/*! Segment reception, identified through a file template, service ID is in evt_param*/
	GF_ROUTE_EVT_DYN_SEG,
    /*! fragment reception (part of a segment), identified through a file template, service ID is in evt_param
     \note The data is always beginning at the start of the object
    */
    GF_ROUTE_EVT_DYN_SEG_FRAG,
    /*! Object deletion (only for dynamic TOIs), used to notify the cache that an object is no longer available. File info only contains the filename being removed*/
    GF_ROUTE_EVT_FILE_DELETE,
} GF_ROUTEEventType;

enum
{
	/*No-operation extension header*/
	GF_LCT_EXT_NOP = 0,
	/*Authentication extension header*/
	GF_LCT_EXT_AUTH = 1,
	/*Time extension header*/
	GF_LCT_EXT_TIME = 2,
	/*FEC object transmission information extension header*/
	GF_LCT_EXT_FTI = 64,
	/*Extension header for FDT - FLUTE*/
	GF_LCT_EXT_FDT = 192,
	/*Extension header for FDT content encoding - FLUTE*/
	GF_LCT_EXT_CENC = 193,
	/*TOL extension header - ROUTE - 24 bit payload*/
	GF_LCT_EXT_TOL24 = 194,
	/*TOL extension header - ROUTE - HEL + 28 bit payload*/
	GF_LCT_EXT_TOL48 = 67,
};

/*! LCT fragment information*/
typedef struct
{
    /*! offset in bytes of fragment in object / file*/
    u32 offset;
    /*! size in bytes of fragment in object / file*/
    u32 size;
} GF_LCTFragInfo;


/*! Structure used to communicate file objects properties to the user*/
typedef struct
{
	/*! original file name*/
	const char *filename;
	/*! blob data pointer*/
	GF_Blob *blob;
    /*! total size of object if known, 0 otherwise*/
    u32 total_size;
	/*! object TSI*/
	u32 tsi;
	/*! object TOI*/
	u32 toi;
	/*! download time in ms*/
	u32 download_ms;
	/*! flag set if file content has been modified - not set for GF_ROUTE_EVT_DYN_SEG (always true)*/
	Bool updated;
    
    /*! number of fragments, only set for GF_ROUTE_EVT_DYN_SEG*/
    u32 nb_frags;
    /*! fragment info, only set for GF_ROUTE_EVT_DYN_SEG*/
    GF_LCTFragInfo *frags;
	
	/*! user data set to current object after callback, and passed back on next callbacks on same object
	 Only used for GF_ROUTE_EVT_FILE, GF_ROUTE_EVT_DYN_SEG, GF_ROUTE_EVT_DYN_SEG_FRAG and GF_ROUTE_EVT_FILE_DELETE
	 */
	void *udta;
} GF_ROUTEEventFileInfo;

/*! Creates a new ROUTE ATSC3.0 demultiplexer
\param ifce network interface to monitor, NULL for INADDR_ANY
\param sock_buffer_size default buffer size for the udp sockets. If 0, uses 0x2000
\param on_event the user callback function
\param udta the user data passed back by the callback
\return the ROUTE demultiplexer created
*/
GF_ROUTEDmx *gf_route_atsc_dmx_new(const char *ifce, u32 sock_buffer_size, void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo), void *udta);


/*! Creates a new ROUTE ATSC3.0 demultiplexer
\param ifce network interface to monitor, NULL for INADDR_ANY
\param sock_buffer_size default buffer size for the udp sockets. If 0, uses 0x2000
\param netcap_id ID of netcap configuration to use, may be null (see gpac -h netcap)
\param on_event the user callback function
\param udta the user data passed back by the callback
\return the ROUTE demultiplexer created
*/
GF_ROUTEDmx *gf_route_atsc_dmx_new_ex(const char *ifce, u32 sock_buffer_size, const char *netcap_id, void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo), void *udta);


/*! Creates a new ROUTE demultiplexer
\param ip IP address of ROUTE session
\param port port of ROUTE session
\param ifce network interface to monitor, NULL for INADDR_ANY
\param sock_buffer_size default buffer size for the udp sockets. If 0, uses 0x2000
\param on_event the user callback function
\param udta the user data passed back by the callback
\return the ROUTE demultiplexer created
*/
GF_ROUTEDmx *gf_route_dmx_new(const char *ip, u32 port, const char *ifce, u32 sock_buffer_size, void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo), void *udta);


/*! Creates a new ROUTE demultiplexer
\param ip IP address of ROUTE session
\param port port of ROUTE session
\param ifce network interface to monitor, NULL for INADDR_ANY
\param sock_buffer_size default buffer size for the udp sockets. If 0, uses 0x2000
\param netcap_id ID of netcap configuration to use, may be null (see gpac -h netcap)
\param on_event the user callback function
\param udta the user data passed back by the callback
\return the ROUTE demultiplexer created
*/
GF_ROUTEDmx *gf_route_dmx_new_ex(const char *ip, u32 port, const char *ifce, u32 sock_buffer_size, const char *netcap_id, void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo), void *udta);

/*! Deletes an ROUTE demultiplexer
\param routedmx the ROUTE demultiplexer to delete
*/
void gf_route_dmx_del(GF_ROUTEDmx *routedmx);

/*! Processes demultiplexing, returns when nothing to read
\param routedmx the ROUTE demultiplexer
\return error code if any, GF_IP_NETWORK_EMPTY if nothing was read
 */
GF_Err gf_route_dmx_process(GF_ROUTEDmx *routedmx);


/*! Sets reordering on.
\param routedmx the ROUTE demultiplexer
\param force_reorder if TRUE, the order flag in ROUTE/LCT is ignored and objects are gathered for the given time. Otherwise, if order flag is set in ROUTE/LCT, an object is considered done as soon as a new object starts
\param timeout_ms maximum delay to wait before considering the object is done when ROUTE/LCT order is not used. A value of 0 implies waiting forever (default value is 5s).
\return error code if any
 */
GF_Err gf_route_set_reorder(GF_ROUTEDmx *routedmx, Bool force_reorder, u32 timeout_ms);

/*! Allow segments to be sent while being downloaded.
 
\note Files with a static TOI association are always sent once completely received, other files using TOI templating may be sent while being received if enabled. The data sent is always contiguous data since the beginning of the file in that case.
 
\param routedmx the ROUTE demultiplexer
\param allow_progressive if TRUE,  fragments of segments will be sent during download
\return error code if any
 */
GF_Err gf_route_set_allow_progressive_dispatch(GF_ROUTEDmx *routedmx, Bool allow_progressive);

/*! Sets the service ID to tune into for ATSC 3.0
\param routedmx the ROUTE demultiplexer
\param service_id ID of the service to tune in. 0 means no service, 0xFFFFFFFF means all services and 0xFFFFFFFE means first service found
\param tune_others if set, will tune all non-selected services to get the MPD, but won't receive any media data
\return error code if any
 */
GF_Err gf_route_atsc3_tune_in(GF_ROUTEDmx *routedmx, u32 service_id, Bool tune_others);


/*! Gets the number of objects currently loaded in the service
\param routedmx the ROUTE demultiplexer
\param service_id ID of the service to query
\return number of objects in service
 */
u32 gf_route_dmx_get_object_count(GF_ROUTEDmx *routedmx, u32 service_id);

/*! Removes an object with a given filename
\param routedmx the ROUTE demultiplexer
\param service_id ID of the service to query
\param fileName name of the file associated with the object
\param purge_previous if set, indicates that all objects with the same TSI and a TOI less than TOI of the deleted object will be removed
\return error if any, GF_NOT_FOUND if no such object
 */
GF_Err gf_route_dmx_remove_object_by_name(GF_ROUTEDmx *routedmx, u32 service_id, char *fileName, Bool purge_previous);

/*! Flags an object to be kept until \ref gf_route_dmx_remove_object_by_name is called
\param routedmx the ROUTE demultiplexer
\param service_id ID of the service to query
\param fileName name of the file associated with the object
\return error if any, GF_NOT_FOUND if no such object
 */
GF_Err gf_route_dmx_force_keep_object_by_name(GF_ROUTEDmx *routedmx, u32 service_id, char *fileName);

/*! Removes the first object loaded in the service
\param routedmx the ROUTE demultiplexer
\param service_id ID of the service to query
\return GF_TRUE if success, GF_FALSE if no object could be removed (the object is in download)
 */
Bool gf_route_dmx_remove_first_object(GF_ROUTEDmx *routedmx, u32 service_id);

/*! Checks existence of a service for atsc 3.0
\param routedmx the ROUTE demultiplexer
\param service_id ID of the service to query
\return true if service is found, false otherwise
 */
Bool gf_route_dmx_find_atsc3_service(GF_ROUTEDmx *routedmx, u32 service_id);

/*! Removes all non-signaling objects (ie TSI!=0), keeping only init segments and currently/last downloaded objects
\note this is mostly useful in case of looping session, or at MPD switch boundaries
\param routedmx the ROUTE demultiplexer
\param service_id ID of the service to cleanup
 */
void gf_route_dmx_purge_objects(GF_ROUTEDmx *routedmx, u32 service_id);


/*! Gets high resolution system time clock of the first packet received
\param routedmx the ROUTE demultiplexer
\return system clock in microseconds of first packet received
 */
u64 gf_route_dmx_get_first_packet_time(GF_ROUTEDmx *routedmx);

/*! Gets high resolution system time clock of the last packet received
\param routedmx the ROUTE demultiplexer
\return system clock in microseconds of last packet received
 */
u64 gf_route_dmx_get_last_packet_time(GF_ROUTEDmx *routedmx);

/*! Gets the number of packets received since start of the session, for all active services
\param routedmx the ROUTE demultiplexer
\return number of packets received
 */
u64 gf_route_dmx_get_nb_packets(GF_ROUTEDmx *routedmx);

/*! Gets the number of bytes received since start of the session, for all active services
\param routedmx the ROUTE demultiplexer
\return number of bytes received
 */
u64 gf_route_dmx_get_recv_bytes(GF_ROUTEDmx *routedmx);

/*! Gather only  objects with given TSI (for debug purposes)
\param routedmx the ROUTE demultiplexer
\param tsi the target TSI, 0 for no filtering
 */
void gf_route_dmx_debug_tsi(GF_ROUTEDmx *routedmx, u32 tsi);

/*! Sets udta for given service id
\param routedmx the ROUTE demultiplexer
\param service_id the target service
\param udta the target user data
 */
void gf_route_dmx_set_service_udta(GF_ROUTEDmx *routedmx, u32 service_id, void *udta);

/*! Gets udta for given service id
\param routedmx the ROUTE demultiplexer
\param service_id the target service
\return the user data associated with the service
 */
void *gf_route_dmx_get_service_udta(GF_ROUTEDmx *routedmx, u32 service_id);

/*! @} */
#ifdef __cplusplus
}
#endif

#endif /* GPAC_DISABLE_ROUTE */

#endif	//_GF_ROUTE_H_

