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

/*!
 *	\file <gpac/network.h>
 *	\brief IP network functions.
 */

 /*!
 *	\addtogroup net_grp network
 *	\ingroup utils_grp
 *	\brief IP Network Functions
 *
 *This section documents the IP network functions of the GPAC framework.
 *	@{
 */

#include <gpac/tools.h>


/*!
 *\brief URL local test
 *
 *Tests whether a URL describes a local file or not
 *\param url the url to analyze
 *\return 1 if the URL describes a local file, 0 otherwise
 */
Bool gf_url_is_local(const char *url);

/*!
 *\brief gets absolute file path
 *
 *Gets the absolute file path from a relative path and its parent absolute one. This can only be used with file paths.
 *\param pathName the relative path name of a file
 *\param parentPath the absolute parent path name 
 *\return absolute path name of the file, or NULL if bad paths are provided.
 \note the returned string must be freed by user
 */
char *gf_url_get_absolute_path(const char *pathName, const char *parentPath);
/*concatenates URL and gets back full URL - returned string must be freed by user*/
/*!
 *\brief URL concatenation
 *
 *Concatenates a relative URL with its parent URL
 *\param parentName URL of the parent service
 *\param pathName URL of the service
 *\return absolute path name of the service, or NULL if bad paths are provided or if the service path is already an absolute one.
 \note the returned string must be freed by user
 */
char *gf_url_concatenate(const char *parentName, const char *pathName);


/*!
 *\brief gets UTC time 
 *
 *Gets UTC time since midnight Jan 1970
 *\param sec number of seconds
 *\param msec number of milliseconds
 */
void gf_utc_time_since_1970(u32 *sec, u32 *msec);

/*!
 *\brief gets NTP time 
 *
 *Gets NTP (Network Time Protocol) in seconds and fractional side
 \param sec NTP time in seconds
 \param frac fractional NTP time expressed in 1 / (1<<32 - 1) seconds units
 */
void gf_get_ntp(u32 *sec, u32 *frac);



/*!
 *\brief abstracted socket object
 *
 *The abstracted socket object allows you to build client and server applications very simply
 *with support for unicast and multicast (no IPv6 yet)
*/
typedef struct __tag_socket GF_Socket;

/*!Buffer size to pass for IP address retrieval*/
#define GF_MAX_IP_NAME_LEN	516

/*!socket is a UDP socket*/
#define GF_SOCK_TYPE_UDP		0x01
/*!socket is a TCP socket*/
#define GF_SOCK_TYPE_TCP		0x02

/*!
 *\brief socket constructor
 *
 *Constructs a socket object
 *\param SocketType the socket type to create, either UDP or TCP
 *\return the socket object or NULL if network initialization failure
 */
GF_Socket *gf_sk_new(u32 SocketType);
/*!
 *\brief socket destructor
 *
 *Deletes a socket object
 *\param sock the socket object
 */
void gf_sk_del(GF_Socket *sock);

/*!
 *\brief reset internal buffer
 *
 *Forces the internal socket buffer to be reseted (discarded)
 *\param sock the socket object
 */
void gf_sk_reset(GF_Socket *sock);
/*!
 *\brief socket buffer size control
 *
 *Sets the size of the internal buffer of the socket. 
 *\param sock the socket object
 *\param send_buffer if 0, sets the size of the reception buffer, otherwise sets the size of the emission buffer
 *\param new_size new size of the buffer in bytes.
 *\warning This operation may fail depending on the provider, hardware...
 */
GF_Err gf_sk_set_buffer_size(GF_Socket *sock, Bool send_buffer, u32 new_size);

/*!
 *\brief blocking mode control
 *
 *Sets the blocking mode of a socket on or off. A blocking socket will wait for the net operation to be possible 
 *while a non-blocking one would return an error. By default, sockets are created in blocking mode
 *\param sock the socket object
 *\param NonBlockingOn set to 1 to use on-blocking sockets, 0 otherwise
 */
GF_Err gf_sk_set_blocking(GF_Socket *sock, Bool NonBlockingOn);
/*!
 *\brief socket binding
 *
 *Binds the given socket to the specified port.
 *\param sock the socket object
 *\param port port number to bind this socket to
 *\param re_use set to 1 to enable reuse of ports on a single machine, 0 otherwise
 */
GF_Err gf_sk_bind(GF_Socket *sock, u16 port, Bool re_use);
/*!
 *\brief connects a socket 
 *
 *Connetcs a socket to a remote peer on a given port 
 *\param sock the socket object
 *\param peer_name the remote server address (IP or DNS)
 *\param port remote port number to connect the socket to
 */
GF_Err gf_sk_connect(GF_Socket *sock, char *peer_name, u16 port);
/*!
 *\brief data emission
 *
 *Sends a buffer on the socket. The socket must be in a bound or connected mode
 *\param sock the socket object
 *\param buffer the data buffer to send
 *\param length the data length to send
 */
GF_Err gf_sk_send(GF_Socket *sock, unsigned char *buffer, u32 length);
/*!
 *\brief data reception
 * 
 *Fetches data on a socket. The socket must be in a bound or connected state
 *\param sock the socket object
 *\param buffer the recpetion buffer where data is written
 *\param length the allocated size of the reception buffer
 *\param start_from the offset in the reception buffer where to start writing
 *\param read the actual number of bytes received
 */
GF_Err gf_sk_receive(GF_Socket *sock, unsigned char *buffer, u32 length, u32 start_from, u32 *read);
/*!
 *\brief socket listening
 *
 *Sets the socket in a listening state. This socket must have been bound to a port before 
 *\param sock the socket object
 *\param max_conn the maximum number of simultaneous connection this socket will accept
 */
GF_Err gf_sk_listen(GF_Socket *sock, u32 max_conn);
/*!
 *\brief socket accept
 *
 *Accepts an incomming connection on a listening socket
 *\param sock the socket object
 *\param new_conn the resulting connection socket object
 */
GF_Err gf_sk_accept(GF_Socket *sock, GF_Socket **new_conn);

/*!
 *\brief server socket mode 
 *
 *Disable the Nable algo (e.g. set TCP_NODELAY) and set the KEEPALIVE on 
 *\param sock the socket object
 *\param server_on sets server mode on or off
*/
GF_Err gf_sk_server_mode(GF_Socket *sock, Bool server_on);

/*!
 *\brief get local host name
 *
 *Retrieves local host name.
 *\param buffer destination buffer for name. Buffer must be GF_MAX_IP_NAME_LEN long
 */
GF_Err gf_sk_get_host_name(char *buffer);

/*!
 *\brief get local IP
 *
 *Gets local IP address of a connected socket, typically used for server after an ACCEPT
 *\param sock the socket object
 *\param buffer destination buffer for IP address. Buffer must be GF_MAX_IP_NAME_LEN long
 */
GF_Err gf_sk_get_local_ip(GF_Socket *sock, char *buffer);
/*!
 *\brief get local info
 *
 *Gets local socket info of a socket
 *\param sock the socket object
 *\param port local port number of the socket
 *\param sock_type socket type (UDP or TCP)
 */
GF_Err gf_sk_get_local_info(GF_Socket *sock, u16 *port, u32 *sock_type);

/*!
 *\brief get remote address
 *
 *Gets the remote address of a peer. The socket MUST be connected.
 *\param sock the socket object
 *\param buffer destination buffer for IP address. Buffer must be GF_MAX_IP_NAME_LEN long
 */
GF_Err gf_sk_get_remote_address(GF_Socket *sock, char *buffer);

/*!
 *\brief set remote address
 *
 *Sets the remote address of a socket. This is used by connectionless sockets using SendTo and RecieveFrom
 *\param sock the socket object
 *\param address the remote peer address
 */
GF_Err gf_sk_set_remote_address(GF_Socket *sock, char *address);
/*!
 *\brief set remote port
 *
 *Sets the remote port of a socket. This is used by connectionless sockets using SendTo and RecieveFrom
 *\param sock the socket object
 *\param port the remote peer port
 */
GF_Err gf_sk_set_remote_port(GF_Socket *sock, u16 port);

/*!
 *\brief connection-less emission 
 *
 *Sends data to the specified host. 
 *\param sock the socket object
 *\param buffer the data buffer to send
 *\param length the data length to send
 *\param remote_host the remote address to send data to. If NULL, the default host is used (cf \ref gf_sk_set_remote_address)
 *\param remote_port the remote port to send data to. This must be specified if the remote_host is used
 */
GF_Err gf_sk_send_to(GF_Socket *sock, unsigned char *buffer, u32 length, unsigned char *remote_host, u16 remote_port);


/*!
 *\brief multicast setup
 *
 *Performs multicast setup (BIND and JOIN) for the socket object
 *\param sock the socket object
 *\param multi_ip_add the multicast IP address
 *\param multi_port the multicast port number
 *\param TTL the multicast TTL (Time-To-Live)
 *\param no_bind if sets, only join the multicast
 */
GF_Err gf_sk_setup_multicast(GF_Socket *sock, char *multi_ip_add, u16 multi_port, u32 TTL, Bool no_bind);
/*!
 *brief multicast address test
 *
 *tests whether an IP address is a multicast one or not
 *\param multi_ip_add the multicast IP address to test
 *\return 1 if the address is a multicast one, 0 otherwise
 */
u32 gf_sk_is_multicast_address(char *multi_ip_add);

/*!
 *\brief send data with wait delay
 *
 *Sends data with a max wait delay. This is used for http / ftp sockets mainly. The socket must be connected.
 *\param sock the socket object
 *\param buffer the data buffer to send
 *\param length the data length to send
 *\param delay_sec the maximum delay in second to wait before aborting
 *\return If the operation timeed out, the function will return a GF_IP_SOCK_WOULD_BLOCK error.
 */
GF_Err gf_sk_send_wait(GF_Socket *sock, unsigned char *buffer, u32 length, u32 delay_sec);
/* recieve data with a max wait delay of Second - used for http / ftp sockets mainly*/
/*!
 *\brief receive data with wait delay
 *
 *Fetches data with a max wait delay. This is used for http / ftp sockets mainly. The socket must be connected.
 *\param sock the socket object
 *\param buffer the recpetion buffer where data is written
 *\param length the allocated size of the reception buffer
 *\param start_from the offset in the reception buffer where to start writing
 *\param read the actual number of bytes received
 *\param delay_sec the maximum delay in second to wait before aborting
 *\return If the operation timeed out, the function will return a GF_IP_SOCK_WOULD_BLOCK error.
 */
GF_Err gf_sk_receive_wait(GF_Socket *sock, unsigned char *buffer, u32 length, u32 start_from, u32 *read, u32 delay_sec);

/*!
 *\brief gets socket handle
 *
 *Gets the socket low-level handle as used by OpenSSL.
 *\param sock the socket object
 *\return the socket handle
 */
s32 gf_sk_get_handle(GF_Socket *sock);


/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_NET_H_*/

