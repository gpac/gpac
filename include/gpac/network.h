/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#ifndef _GF_NET_H_
#define _GF_NET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>



/*********************************************************************
					URL Manipulation
**********************************************************************/

Bool gf_url_is_local(const char *pathName);

/*gets absolute file path - returned string must be freed by user*/
char *gf_url_get_absolute_path(const char *pathName, const char *parentPath);
/*concatenates URL and gets back full URL - returned string must be freed by user*/
char *gf_url_concatenate(const char *parentName, const char *pathName);


/*gets UTC time since midnight Jan 1970*/
void gf_utc_time_since_1970(u32 *sec, u32 *msec);
/*Get NTP time in sec + fractional side ( in 1 / (1<<32 - 1) sec units)*/
void gf_get_ntp(u32 *sec, u32 *frac);
/*get reduced NTP time on 32 bits (used in RTP a lot)*/
u32 gf_get_ntp_frac(u32 sec, u32 frac);


/*********************************************************************
					Socket Object
**********************************************************************/

#define GF_MAX_IP_NAME_LEN	516

/* socket types */
#define GF_SOCK_TYPE_UDP		0x01
#define GF_SOCK_TYPE_TCP		0x02

typedef struct __tag_socket GF_Socket;

GF_Socket *gf_sk_new(u32 SocketType);
void gf_sk_del(GF_Socket *sock);

/* forces a reset of the socket buffer */
void gf_sk_reset(GF_Socket *sock);
/* set the buffer size for the socket. If SendBuffer is 0, set the recieving buffer */
GF_Err gf_sk_set_buffer_size(GF_Socket *sock, Bool SendBuffer, u32 NewSize);

/* set the blocking mode of a socket on or off. By default, sockets
are created in blocking mode (BSD style) */
GF_Err gf_sk_set_blocking(GF_Socket *sock, u32 NonBlockingOn);
/* binds the given socket to the specified port. If ReUse is true
this will enable reuse of ports on a single machine */
GF_Err gf_sk_bind(GF_Socket *sock, u16 PortNumber, Bool reUse);
/*connects a socket to a remote peer on a given port */
GF_Err gf_sk_connect(GF_Socket *sock, char *PeerName, u16 PortNumber);
/* send a buffer on the socket */
GF_Err gf_sk_send(GF_Socket *sock, unsigned char *buffer, u32 length);
/* fetch nb bytes on a socket and fill the buffer from startFrom */
GF_Err gf_sk_receive(GF_Socket *sock, unsigned char *buffer, u32 length, u32 startFrom, u32 *BytesRead);

/* make the specified socket listen. This socket MUST have been bound to a port before */
Bool gf_sk_listen(GF_Socket *sock, u32 MaxConnection);
/* accept an incomming connection */
GF_Err gf_sk_accept(GF_Socket *sock, GF_Socket **newConnection);

/* disable the Nable algo (aka set TCP_NODELAY) and set the KEEPALIVE on */
GF_Err gf_sk_server_mode(GF_Socket *sock, Bool serverOn);

/* buffer must be GF_MAX_IP_NAME_LEN long */
GF_Err gf_sk_get_host_name(char *buffer);
/* Get local IP for connected sockets (typically used for server after an ACCEPT */
GF_Err gf_sk_get_local_ip(GF_Socket *sock, char *buffer);
GF_Err gf_sk_get_local_info(GF_Socket *sock, u16 *Port, u32 *Familly);

/* get the remote address of a peer. Socket MUST be connected
buffer must be GF_MAX_IP_NAME_LEN long */
GF_Err gf_sk_get_remote_address(GF_Socket *sock, char *buffer);

/* Connection-less sockets (UDP server side) to use with SendTo and RecieveFrom */
/* Set the remote address of a socket */
GF_Err gf_sk_set_remote_address(GF_Socket *sock, char *address);
/* Set the remote port for a socket */
GF_Err gf_sk_set_remote_port(GF_Socket *sock, u16 RemotePort);

/* send data to the specified host. If no host is specified (NULL), the
default host (gf_sk_set_remote_address ...) is used */
GF_Err gf_sk_send_to(GF_Socket *sock, unsigned char *buffer, u32 length, unsigned char *remoteHost, u16 remotePort);


/* Performs multicast BIND and JOIN */
GF_Err gf_sk_setup_multicast(GF_Socket *sock, char *multi_IPAdd, u16 MultiPortNumber, u32 TTL, Bool NoBind);
/* returns 1 if multicast address, 0 otherwise*/
u32 gf_sk_is_multicast_address(char *multi_IPAdd);

/* send data with a max wait delay of Second - used for http / ftp sockets mainly*/
GF_Err gf_sk_send_wait(GF_Socket *sock, unsigned char *buffer, u32 length, u32 Second );
/* recieve data with a max wait delay of Second - used for http / ftp sockets mainly*/
GF_Err gf_sk_receive_wait(GF_Socket *sock, unsigned char *buffer, u32 length, u32 startFrom, u32 *BytesRead, u32 Second );

/*returns socket handle (used by SSL)*/
s32 gf_sk_get_handle(GF_Socket *sock);


/*********************************************************************
					Socket Group Object
**********************************************************************/
#define GF_SOCK_GROUP_READ		0x01
#define GF_SOCK_GROUP_WRITE		0x02
#define GF_SOCK_GROUP_ERROR		0x03

/* Socket Group for select(). The group is a collection of sockets
ready for reading / writing */
typedef struct __tag_sock_group GF_SocketGroup;

GF_SocketGroup *gf_sk_group_new();
void gf_sk_group_del(GF_SocketGroup *group);
void gf_sk_group_set_time(GF_SocketGroup *group, u32 DelayInS, u32 DelayInMicroS);

/* call this to add the socket into a group */
void gf_sk_group_add(GF_SocketGroup *group, GF_Socket *sock, u32 GroupType);
/* call this to remove the socket from a group */
void gf_sk_group_rem(GF_SocketGroup *group, GF_Socket *sock, u32 GroupType);


/* call this after a SKG_Select to know if the socket is ready */
Bool gf_sk_group_is_in(GF_SocketGroup *group, GF_Socket *sock, u32 GroupType);
/* select the socket(s) watched in this group. Return the number of pending socket
or 0 if none are ready */
u32 gf_sk_group_select(GF_SocketGroup *group);



#ifdef __cplusplus
}
#endif


#endif		/*_GF_NET_H_*/

