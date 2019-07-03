/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
 *	\brief Networking.
 */

/*!
*	\addtogroup net_grp Network
*	\ingroup utils_grp
*	\brief Networking tools (URL resolution, TCP/UDP sockets)
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
 *\brief error code description
 *
 *Returns text description of given errno code
 *\param errnoval the error value to test
 *\return its description
 */
const char *gf_errno_str(int errnoval);

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
 *\brief URL concatenation
 *
 *Same as \ref gf_url_concatenate but if both paths are relative, resolved url is relative to parent path.
 *\param parentName URL of the parent service
 *\param pathName URL of the service
 *\return absolute path name of the service, or NULL if bad paths are provided or if the service path is already an absolute one.
 \note the returned string must be freed by user
 */
char *gf_url_concatenate_parent(const char *parentName, const char *pathName);

/*!
 *\brief URL encodin
 *
 *Encodes URL by replacing special characters with their % encodings.
 *\param path URL of the service
 *\return encoded path name , or NULL if bad paths are provided.
 \note the returned string must be freed by user
 */
char *gf_url_percent_encode(const char *path);

/*!
 *\brief URL to file system
 *
 *Converts a local URL to a file system value. Removes all white spaces and similar
 *\param url url to convert
 */
void gf_url_to_fs_path(char *url);


/*!
 *\brief Extract resource name from URL
 *
 * Extracts the resource name from the URL
 *\param url input url
 *\return resource name.
 */
const char *gf_url_get_resource_name(const char *url);

/*!
 *\brief Extract resource path from URL
 *
 *Extracts the reource path from the URL
 *\param url input url
 *\param res_path buffer for resulting path storage
 *\return 1 if path was extracted, 0 if url is a single file name.
 */
Bool gf_url_get_resource_path(const char *url, char *res_path);


/*!
 *\brief gets UTC time
 *
 *Gets UTC time since midnight Jan 1970
 *\param sec number of seconds
 *\param msec number of milliseconds
 */
void gf_utc_time_since_1970(u32 *sec, u32 *msec);


/*!
 *	\brief NTP seconds from 1900 to 1970
 *	\hideinitializer
 *
 *	Macro giving the number of seconds from from 1900 to 1970
*/
#define GF_NTP_SEC_1900_TO_1970 2208988800ul

/*!
 *\brief gets NTP time
 *
 *Gets NTP (Network Time Protocol) in seconds and fractional side
 \param sec NTP time in seconds
 \param frac fractional NTP time expressed in 1 / (1<<32 - 1) seconds units
 */
void gf_net_get_ntp(u32 *sec, u32 *frac);

/*!
*\brief gets NTP time in milliseconds
*
\return NTP time in milliseconds
*/
u64 gf_net_get_ntp_ms();

/*!
 *\brief offsets NTP time by a given amount of seconds
 *
 *Offsets NTP time of the system by a given amount of seconds in the future or the past (default value is 0).
 \param sec seconds to add or remove to the system NTP
 */
void gf_net_set_ntp_shift(s32 sec);

/*!
 *\brief gets NTP time
 *
 *Gets NTP (Network Time Protocol) timestamp (high 32 bit is seconds, low 32 bit is fraction)
 \return NTP timestamp
 */
u64 gf_net_get_ntp_ts();

/*!
 *
 *Gets diff in milliseconds between NTP time and current time
 \param ntp NTP timestamp
 \return diff in milliseconds with the current time
 */
s32 gf_net_get_ntp_diff_ms(u64 ntp);

/*!
 * Socket options
 *	\hideinitializer
 */
enum
{
	/*!Reuses port.*/
	GF_SOCK_REUSE_PORT = 1,
	/*!Forces IPV6 if available.*/
	GF_SOCK_FORCE_IPV6 = 1<<1,
	/*!Does not perfom the actual bind, only keeps adress and port.*/
	GF_SOCK_FAKE_BIND = 1<<2
};

/*!
 *\brief abstracted socket object
 *
 *The abstracted socket object allows you to build client and server applications very simply
 *with support for unicast and multicast
*/
typedef struct __tag_socket GF_Socket;

/*!
 *\brief abstracted socket group object
 *
 *The abstracted socket group object allows querying multiple sockets in a group
*/
typedef struct __tag_sock_group GF_SockGroup;

/*!Buffer size to pass for IP address retrieval*/
#define GF_MAX_IP_NAME_LEN	1025

/*!socket is a TCP socket*/
#define GF_SOCK_TYPE_TCP		0x01
/*!socket is a UDP socket*/
#define GF_SOCK_TYPE_UDP		0x02

#ifdef GPAC_HAS_SOCK_UN
/*!socket is a TCP socket*/
#define GF_SOCK_TYPE_TCP_UN		0x03
/*!socket is a TCP socket*/
#define GF_SOCK_TYPE_UDP_UN		0x04
#endif

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
 *Sets the size of the internal buffer of the socket. The socket MUST be bound or connected before.
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
GF_Err gf_sk_set_block_mode(GF_Socket *sock, Bool NonBlockingOn);
/*!
 *\brief socket binding
 *
 *Binds the given socket to the specified port.
 *\param local_ip the local interface IP address if desired. If NULL, the default interface will be used.
 *\param sock the socket object
 *\param port port number to bind this socket to
 *\param peer_name the remote server address, if NULL, will use localhost
 *\param peer_port remote port number to connect the socket to
 *\param options list of option for the bind operation.
 */
GF_Err gf_sk_bind(GF_Socket *sock, const char *local_ip, u16 port, const char *peer_name, u16 peer_port, u32 options);
/*!
 *\brief connects a socket
 *
 *Connects a socket to a remote peer on a given port
 *\param sock the socket object
 *\param peer_name the remote server address (IP or DNS)
 *\param port remote port number to connect the socket to
 *\param local_ip the local (client) address (IP or DNS) if any, NULL otherwise.
 */
GF_Err gf_sk_connect(GF_Socket *sock, const char *peer_name, u16 port, const char *local_ip);
/*!
 *\brief data emission
 *
 *Sends a buffer on the socket. The socket must be in a bound or connected mode
 *\param sock the socket object
 *\param buffer the data buffer to send
 *\param length the data length to send
 */
GF_Err gf_sk_send(GF_Socket *sock, const u8 *buffer, u32 length);
/*!
 *\brief data reception
 *
 *Fetches data on a socket. The socket must be in a bound or connected state
 *\param sock the socket object
 *\param buffer the reception buffer where data is written. Passing NULL will only probe for data in socket (eg only do select() ).
 *\param length the allocated size of the reception buffer
 *\param read the actual number of bytes received
 *\return error if any, GF_IP_NETWORK_EMPTY if nothing to read
 */
GF_Err gf_sk_receive(GF_Socket *sock, u8 *buffer, u32 length, u32 *read);

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
 *Accepts an incoming connection on a listening socket
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
 *Sets the remote address of a socket. This is used by connectionless sockets using SendTo and ReceiveFrom
 *\param sock the socket object
 *\param address the remote peer address
 *\param port the remote peer port
 */
GF_Err gf_sk_set_remote(GF_Socket *sock, char *address, u16 port);


/*!
 *\brief multicast setup
 *
 *Performs multicast setup (BIND and JOIN) for the socket object
 *\param sock the socket object
 *\param multi_ip_add the multicast IP address
 *\param multi_port the multicast port number
 *\param TTL the multicast TTL (Time-To-Live)
 *\param no_bind if sets, only join the multicast
 *\param local_interface_ip the local interface IP address if desired. If NULL, the default interface will be used.
 */
GF_Err gf_sk_setup_multicast(GF_Socket *sock, const char *multi_ip_add, u16 multi_port, u32 TTL, Bool no_bind, char *local_interface_ip);
/*!
 *brief multicast address test
 *
 *tests whether an IP address is a multicast one or not
 *\param multi_ip_add the multicast IP address to test
 *\return 1 if the address is a multicast one, 0 otherwise
 */
u32 gf_sk_is_multicast_address(const char *multi_ip_add);

/*!
 *\brief send data with wait delay
 *
 *Sends data with a max wait delay. This is used for http / ftp sockets mainly. The socket must be connected.
 *\param sock the socket object
 *\param buffer the data buffer to send
 *\param length the data length to send
 *\param delay_sec the maximum delay in second to wait before aborting
 *\return If the operation timed out, the function will return a GF_IP_SOCK_WOULD_BLOCK error.
 */
GF_Err gf_sk_send_wait(GF_Socket *sock, const u8 *buffer, u32 length, u32 delay_sec);
/* receive data with a max wait delay of Second - used for http / ftp sockets mainly*/
/*!
 *\brief receive data with wait delay
 *
 *Fetches data with a max wait delay. This is used for http / ftp sockets mainly. The socket must be connected.
 *\param sock the socket object
 *\param buffer the reception buffer where data is written
 *\param length the allocated size of the reception buffer
 *\param read the actual number of bytes received
 *\param delay_sec the maximum delay in second to wait before aborting
 *\return If the operation timed out, the function will return a GF_IP_SOCK_WOULD_BLOCK error.
 */
GF_Err gf_sk_receive_wait(GF_Socket *sock, u8 *buffer, u32 length, u32 *read, u32 delay_sec);

/*!
 *\brief gets socket handle
 *
 *Gets the socket low-level handle as used by OpenSSL.
 *\param sock the socket object
 *\return the socket handle
 */
s32 gf_sk_get_handle(GF_Socket *sock);

/*!
 *Sets the socket wait time in microseconds. Default wait time is 500 microseconds. Any value >= 1000000 will reset to default.
 *\param sock the socket object
 *\param usec_wait wait time in microseconds
 */
void gf_sk_set_usec_wait(GF_Socket *sock, u32 usec_wait);

/*!
 *Creates a new socket group
 *\return socket group object
 */
GF_SockGroup *gf_sk_group_new();
/*!
 *Deletes a socket group
 *\param sg socket group object
 */
void gf_sk_group_del(GF_SockGroup *sg);
/*!
 *Registers a socket to a socket group
 *\param sg socket group object
 *\param sk socket object to register
 */
void gf_sk_group_register(GF_SockGroup *sg, GF_Socket *sk);
/*!
 *Unregisters a socket from a socket group
 *\param sg socket group object
 *\param sk socket object to unregister
 */
void gf_sk_group_unregister(GF_SockGroup *sg, GF_Socket *sk);

/*!
 *Performs a select (wait) on the socket group
 *\param sg socket group object
 *\param wait_usec microseconds to wait (can be larger than one second)
 *\return error if any
 */
GF_Err gf_sk_group_select(GF_SockGroup *sg, u32 wait_usec);
/*!
 *Checks if given socket is selected and can be read. This shall be called after gf_sk_group_select
 *\param sg socket group object
 *\param sk socket object to check
 *\return GF_TRUE if socket is ready to read, 0 otherwise
 */
Bool gf_sk_group_sock_is_set(GF_SockGroup *sg, GF_Socket *sk);

/*!
 *Fetches data on a socket without performing any select (wait), to be used with socket group on sockets that are set in the selected socket group
 *\param sock the socket object
 *\param buffer the reception buffer where data is written
 *\param length the allocated size of the reception buffer
 *\param read the actual number of bytes received
 *\return error if any, GF_IP_NETWORK_EMPTY if nothing to read
 */
GF_Err gf_sk_receive_no_select(GF_Socket *sock, u8 *buffer, u32 length, u32 *read);


/*!
 *\brief gets ipv6 support
 *
 *Returns IPV6 support information.
 *\return 2 if the machine has IPV6 support, 1 if the library was compiled with IPV6 support, 0 otherwise
 */
u32 gf_net_has_ipv6();


/*!
 *\brief checks address type
 *
 *Checks if an address is an IPV6 or IPV4 one.
 *\param address Adress to check
 *\return true 1 if address is IPV6 one, 0 otherwise
 */
Bool gf_net_is_ipv6(const char *address);


/*!
 *host to network conversion of integer
 *
 *\param val integrer to convert
 *\return converted integer
 */
u32 gf_htonl(u32 val);
/*!
 *network to host conversion of integer
 *
 *\param val integrer to convert
 *\return converted integer
 */
u32 gf_ntohl(u32 val);
/*!
 *host to network conversion of short integer
 *
 *\param val short integrer to convert
 *\return converted integer
 */
u16 gf_htons(u16 val);
/*!
 *network to host conversion of short integer
 *
 *\param val short integrer to convert
 *\return converted integer
 */
u16 gf_tohs(u16 val);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_NET_H_*/

