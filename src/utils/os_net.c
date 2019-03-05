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

#ifndef GPAC_DISABLE_CORE_TOOLS

#if defined(WIN32) || defined(_WIN32_WCE)

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef _WIN32_WCE
#include <winsock.h>

#if !defined(__GNUC__)
#pragma comment(lib, "winsock")
#endif

#else

#include <sys/timeb.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#if !defined(__GNUC__)
#pragma comment(lib, "ws2_32")
#endif

#endif

#include <windows.h>

#if !defined(__GNUC__)

#if defined(IPV6_MULTICAST_IF)
#define GPAC_HAS_IPV6 1
#pragma message("Using WinSock IPV6")
#else
#undef GPAC_HAS_IPV6
#pragma message("Using WinSock IPV4")
#endif

#endif

#ifndef EISCONN
/*common win32 redefs*/
#undef EAGAIN
#define EAGAIN				WSAEWOULDBLOCK
#define EISCONN				WSAEISCONN
#define ENOTCONN			WSAENOTCONN
#define ECONNRESET			WSAECONNRESET
#define EMSGSIZE			WSAEMSGSIZE
#define ECONNABORTED		WSAECONNABORTED
#define ENETDOWN			WSAENETDOWN
#undef EINTR
#define EINTR				WSAEINTR
#undef EBADF
#define EBADF				WSAEBADF
#endif


#define LASTSOCKERROR WSAGetLastError()

/*the number of sockets used. This because the WinSock lib needs init*/
static int wsa_init = 0;


#include <gpac/network.h>


/*end-win32*/

#else
/*non-win32*/
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#ifndef __BEOS__
#include <errno.h>
#endif

#ifndef __DARWIN__
#include <sys/time.h>
#endif

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <gpac/network.h>

/*not defined on solaris*/
#if !defined(INADDR_NONE)
# if (defined(sun) && defined(__SVR4))
#  define INADDR_NONE -1
# else
#  define INADDR_NONE ((unsigned long)-1)
# endif
#endif


#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define LASTSOCKERROR errno

typedef s32 SOCKET;
#define closesocket(v) close(v)

#endif /*WIN32||_WIN32_WCE*/


#ifdef GPAC_HAS_IPV6
# ifndef IPV6_ADD_MEMBERSHIP
#  define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
# endif
# ifndef IPV6_DROP_MEMBERSHIP
#  define IPV6_DROP_MEMBERSHIP   IPV6_LEAVE_GROUP
# endif
#endif


#ifdef __SYMBIAN32__
#define SSO_CAST
#else
#define SSO_CAST (const char *)
#endif


#ifdef GPAC_HAS_IPV6
static u32 ipv6_check_state = 0;
#endif

#ifdef _LP64
#define NULL_SOCKET 0
#else
#define NULL_SOCKET (SOCKET)NULL
#endif


/*internal flags*/
enum
{
	GF_SOCK_IS_TCP = 1<<9,
	GF_SOCK_IS_IPV6 = 1<<10,
	GF_SOCK_NON_BLOCKING = 1<<11,
	GF_SOCK_IS_MULTICAST = 1<<12,
	GF_SOCK_IS_LISTENING = 1<<13,
	/*socket is bound to a specific dest (server) or source (client) */
	GF_SOCK_HAS_PEER = 1<<14,
	GF_SOCK_IS_MIP = 1<<15
};

struct __tag_socket
{
	u32 flags;
	SOCKET socket;
	/*destination address for sendto/recvfrom*/
#ifdef GPAC_HAS_IPV6
	struct sockaddr_storage dest_addr;
#else
	struct sockaddr_in dest_addr;
#endif
	u32 dest_addr_len;

	u32 usec_wait;
};



/*
	MobileIP tools
*/

static gf_net_mobileip_ctrl_cbk mobip_cbk = NULL;
static const char *MobileIPAdd = NULL;

GF_EXPORT
void gf_net_mobileip_set_callback(gf_net_mobileip_ctrl_cbk _mobip_cbk, const char *mip)
{
	mobip_cbk = _mobip_cbk;
	MobileIPAdd = _mobip_cbk ? mip : NULL;
}

static GF_Err gf_net_mobileip_ctrl(Bool start)
{
	if (mobip_cbk) return mobip_cbk(start);
	return GF_NOT_SUPPORTED;
}

GF_EXPORT
u32 gf_net_has_ipv6()
{
#ifdef GPAC_HAS_IPV6
	if (!ipv6_check_state) {
		SOCKET s;
#ifdef WIN32
		if (!wsa_init) {
			WSADATA Data;
			if (WSAStartup(0x0202, &Data)!=0) {
				ipv6_check_state = 1;
				return 0;
			}
		}
#endif
		s = socket(PF_INET6, SOCK_STREAM, 0);
		if (!s) ipv6_check_state = 1;
		else {
			ipv6_check_state = 2;
			closesocket(s);
		}
#ifdef WIN32
		if (!wsa_init) WSACleanup();
#endif
	}
	return (ipv6_check_state==2);
#else
	return 0;
#endif
}

GF_EXPORT
Bool gf_net_is_ipv6(const char *address)
{
	char *sep;
	if (!address) return GF_FALSE;
	sep = strchr(address, ':');
	if (sep) sep = strchr(address, ':');
	return sep ? GF_TRUE : GF_FALSE;
}

#ifdef GPAC_HAS_IPV6
#define MAX_PEER_NAME_LEN 1024
static struct addrinfo *gf_sk_get_ipv6_addr(const char *PeerName, u16 PortNumber, int family, int flags, int sock_type)
{
	struct	addrinfo *res=NULL;
	struct	addrinfo hints;
	char node[MAX_PEER_NAME_LEN], portstring[20];
	char *service, *dest;
	service = dest = NULL;
#ifdef WIN32
	if (!wsa_init) {
		WSADATA Data;
		if (WSAStartup(0x0202, &Data)!=0) return NULL;
		wsa_init = 1;
	}
#endif

	service = dest = NULL;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = sock_type;
	hints.ai_family = family;
	hints.ai_flags = flags;

	if (PortNumber) {
		sprintf (portstring, "%d", PortNumber);
		service = (char *)portstring;
	}
	if (PeerName) {
		strncpy(node, PeerName, MAX_PEER_NAME_LEN);
		if (node[0]=='[') {
			node[strlen(node)-1] = 0;
			strncpy(node, &node[1], MAX_PEER_NAME_LEN);
		}
		node[MAX_PEER_NAME_LEN - 1] = 0;
		dest = (char *) node;
	}
	if (getaddrinfo((const char *)dest, (const char *)service, &hints, &res) != 0) return NULL;
	return res;
}

static Bool gf_sk_ipv6_set_remote_address(GF_Socket *sock, const char *address, u16 PortNumber)
{
	struct addrinfo *res = gf_sk_get_ipv6_addr(address, PortNumber, AF_UNSPEC, 0, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM);
	if (!res) return GF_FALSE;
	memcpy(&sock->dest_addr, res->ai_addr, res->ai_addrlen);
	sock->dest_addr_len = (u32) res->ai_addrlen;
	freeaddrinfo(res);
	return GF_TRUE;
}
#endif


GF_EXPORT
GF_Err gf_sk_get_host_name(char *buffer)
{
	s32 ret = gethostname(buffer, GF_MAX_IP_NAME_LEN);
	return (ret == SOCKET_ERROR) ? GF_IP_ADDRESS_NOT_FOUND : GF_OK;
}

GF_EXPORT
GF_Err gf_sk_get_local_ip(GF_Socket *sock, char *buffer)
{
#ifdef GPAC_HAS_IPV6
	char clienthost[NI_MAXHOST];
	if (sock->flags & GF_SOCK_HAS_PEER) {
		if (getnameinfo((struct sockaddr *)&sock->dest_addr, sock->dest_addr_len, clienthost, sizeof(clienthost), NULL, 0, NI_NUMERICHOST))
			return GF_IP_NETWORK_FAILURE;
	} else {
		struct sockaddr_storage clientaddr;
		socklen_t addrlen = sizeof(clientaddr);
		if (getsockname(sock->socket, (struct sockaddr *)&clientaddr, &addrlen)) return GF_IP_NETWORK_FAILURE;

		if (getnameinfo((struct sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), NULL, 0, NI_NUMERICHOST))
			return GF_IP_NETWORK_FAILURE;
	}
	strcpy(buffer, clienthost);
#else
	char *ip;
	if (sock->flags & GF_SOCK_HAS_PEER) {
		ip = inet_ntoa(sock->dest_addr.sin_addr);
	} else {
		struct sockaddr_in name;
		u32 len = sizeof(struct sockaddr_in);
		if (getsockname(sock->socket, (struct sockaddr*) &name, &len)) return GF_IP_NETWORK_FAILURE;
		ip = inet_ntoa(name.sin_addr);
	}
	if (!ip) return GF_IP_NETWORK_FAILURE;
	strcpy(buffer, ip);
#endif
	return GF_OK;
}


GF_EXPORT
GF_Socket *gf_sk_new(u32 SocketType)
{
	GF_Socket *tmp;

	/*init WinSock*/
#ifdef WIN32
	WSADATA Data;
	if (!wsa_init && (WSAStartup(0x0202, &Data)!=0) ) return NULL;
#endif
	if ((SocketType != GF_SOCK_TYPE_UDP) && (SocketType != GF_SOCK_TYPE_TCP)) return NULL;

	GF_SAFEALLOC(tmp, GF_Socket);
	if (!tmp) return NULL;
	if (SocketType == GF_SOCK_TYPE_TCP) tmp->flags |= GF_SOCK_IS_TCP;

#ifdef GPAC_HAS_IPV6
	memset(&tmp->dest_addr, 0, sizeof(struct sockaddr_storage));
#else
	memset(&tmp->dest_addr, 0, sizeof(struct sockaddr_in));
	tmp->dest_addr_len = sizeof(struct sockaddr);
#endif

	tmp->usec_wait = 500;
#ifdef WIN32
	wsa_init ++;
#endif
	return tmp;
}

GF_EXPORT
GF_Err gf_sk_set_buffer_size(GF_Socket *sock, Bool SendBuffer, u32 NewSize)
{
	u32 nsize=0, psize;
	s32 res;
	if (!sock || !sock->socket) return GF_BAD_PARAM;

	if (SendBuffer) {
		res = setsockopt(sock->socket, SOL_SOCKET, SO_SNDBUF, (char *) &NewSize, sizeof(u32) );
	} else {
		res = setsockopt(sock->socket, SOL_SOCKET, SO_RCVBUF, (char *) &NewSize, sizeof(u32) );
	}
	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Socket] Couldn't set socket %s buffer size to %d: %d\n", SendBuffer ? "send" : "receive", NewSize, res));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[Socket] Set socket %s buffer size to %d\n", SendBuffer ? "send" : "receive", NewSize));
	}
	psize = sizeof(u32);
	if (SendBuffer) {
		res = getsockopt(sock->socket, SOL_SOCKET, SO_SNDBUF, (char *) &nsize, &psize );
	} else {
		res = getsockopt(sock->socket, SOL_SOCKET, SO_RCVBUF, (char *) &nsize, &psize );
	}
	if ((res>=0) && (nsize=!NewSize)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Socket] Asked to set socket %s buffer size to %d but system used %d\n", SendBuffer ? "send" : "receive", NewSize, nsize));
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_set_block_mode(GF_Socket *sock, Bool NonBlockingOn)
{
	s32 res;
#ifdef WIN32
	long val = NonBlockingOn;
	if (sock->socket) {
		res = ioctlsocket(sock->socket, FIONBIO, &val);
		if (res) return GF_SERVICE_ERROR;
	}
#else
	s32 flag = fcntl(sock->socket, F_GETFL, 0);
	if (sock->socket) {
		res = fcntl(sock->socket, F_SETFL, flag | O_NONBLOCK);
		if (res) return GF_SERVICE_ERROR;
	}
#endif
	if (NonBlockingOn) {
		sock->flags |= GF_SOCK_NON_BLOCKING;
	} else {
		sock->flags &= ~GF_SOCK_NON_BLOCKING;
	}
	return GF_OK;
}

#include <assert.h>

static void gf_sk_free(GF_Socket *sock)
{
	assert( sock );
	/*leave multicast*/
	if (sock->socket && (sock->flags & GF_SOCK_IS_MULTICAST) ) {
		struct ip_mreq mreq;
#ifdef GPAC_HAS_IPV6
		struct sockaddr *addr = (struct sockaddr *)&sock->dest_addr;
		if (addr->sa_family==AF_INET6) {
			struct ipv6_mreq mreq6;
			memcpy(&mreq6.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)addr)->sin6_addr), sizeof(struct in6_addr));
			mreq6.ipv6mr_interface= 0;
			setsockopt(sock->socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *) &mreq6, sizeof(mreq6));
		} else {
			mreq.imr_multiaddr.s_addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
			mreq.imr_interface.s_addr = INADDR_ANY;
			setsockopt(sock->socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &mreq, sizeof(mreq));
		}
#else
		mreq.imr_multiaddr.s_addr = sock->dest_addr.sin_addr.s_addr;
		mreq.imr_interface.s_addr = INADDR_ANY;
		setsockopt(sock->socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &mreq, sizeof(mreq));
#endif
	}
	if (sock->socket) closesocket(sock->socket);
	sock->socket = (SOCKET) 0L;

	/*if MobileIP socket, unregister it*/
	if (sock->flags & GF_SOCK_IS_MIP) {
		sock->flags &= ~GF_SOCK_IS_MIP;
		gf_net_mobileip_ctrl(GF_FALSE);
	}
}


GF_EXPORT
void gf_sk_del(GF_Socket *sock)
{
	assert( sock );
	gf_sk_free(sock);
#ifdef WIN32
	wsa_init --;
	if (!wsa_init) WSACleanup();
#endif
	gf_free(sock);
}

GF_EXPORT
void gf_sk_reset(GF_Socket *sock)
{
	u32 clear;
	if (sock) setsockopt(sock->socket, SOL_SOCKET, SO_ERROR, (char *) &clear, sizeof(u32) );
}

GF_EXPORT
s32 gf_sk_get_handle(GF_Socket *sock)
{
	return (s32) sock->socket;
}

void gf_sk_set_usec_wait(GF_Socket *sock, u32 usec_wait)
{
	if (!sock) return;
	sock->usec_wait = (usec_wait>=1000000) ? 500 : usec_wait;
}



//connects a socket to a remote peer on a given port
GF_EXPORT
GF_Err gf_sk_connect(GF_Socket *sock, const char *PeerName, u16 PortNumber, const char *local_ip)
{
	s32 ret;
#ifdef GPAC_HAS_IPV6
	u32 type = (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM;
	struct addrinfo *res, *aip, *lip;

	gf_sk_free(sock);

	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Solving %s address\n", PeerName));
	res = gf_sk_get_ipv6_addr(PeerName, PortNumber, AF_UNSPEC, AI_PASSIVE, type);
	if (!res) return GF_IP_CONNECTION_FAILURE;
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Host %s found\n", PeerName));

	/*turn on MobileIP*/
	if (local_ip && MobileIPAdd && !strcmp(MobileIPAdd, local_ip) ) {
		if (gf_net_mobileip_ctrl(GF_TRUE)==GF_OK) {
			sock->flags |= GF_SOCK_IS_MIP;
		} else {
			local_ip = NULL;
		}
	}

	lip = NULL;
	if (local_ip) {
		lip = gf_sk_get_ipv6_addr(local_ip, PortNumber, AF_UNSPEC, AI_PASSIVE, type);
		if (!lip && local_ip) {
			lip = gf_sk_get_ipv6_addr(NULL, PortNumber, AF_UNSPEC, AI_PASSIVE, type);
			local_ip = NULL;
		}
	}

	/*for all interfaces*/
	for (aip=res; aip!=NULL; aip=aip->ai_next) {
		if (type != (u32) aip->ai_socktype) continue;
		sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
		if (sock->socket == INVALID_SOCKET) {
			sock->socket = NULL_SOCKET;
			continue;
		}
		if (sock->flags & GF_SOCK_NON_BLOCKING) gf_sk_set_block_mode(sock, GF_TRUE);
		if (aip->ai_family==PF_INET6) sock->flags |= GF_SOCK_IS_IPV6;
		else sock->flags &= ~GF_SOCK_IS_IPV6;

		if (lip) {
			ret = bind(sock->socket, lip->ai_addr, (int) lip->ai_addrlen);
			if (ret == SOCKET_ERROR) {
				closesocket(sock->socket);
				sock->socket = NULL_SOCKET;
				continue;
			}
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Connecting to %s:%d\n", PeerName, PortNumber));
		ret = connect(sock->socket, aip->ai_addr, (int) aip->ai_addrlen);
		if (ret == SOCKET_ERROR) {
			closesocket(sock->socket);
			sock->socket = NULL_SOCKET;
			continue;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Connected to %s:%d\n", PeerName, PortNumber));

		memcpy(&sock->dest_addr, aip->ai_addr, aip->ai_addrlen);
		sock->dest_addr_len = (u32) aip->ai_addrlen;
		freeaddrinfo(res);
		if (lip) freeaddrinfo(lip);
		return GF_OK;
	}
	freeaddrinfo(res);
	if (lip) freeaddrinfo(lip);
	return GF_IP_CONNECTION_FAILURE;

#else
	struct hostent *Host;

	if (local_ip) {
		/*this will turn on MobileIP if needed*/
		GF_Err e = gf_sk_bind(sock, local_ip, PortNumber, PeerName, PortNumber, GF_SOCK_REUSE_PORT);
		if (e) return e;
	}
	if (!sock->socket) {
		sock->socket = socket(AF_INET, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
		if (sock->flags & GF_SOCK_NON_BLOCKING)
			gf_sk_set_block_mode(sock, 1);
	}

	/*setup the address*/
	sock->dest_addr.sin_family = AF_INET;
	sock->dest_addr.sin_port = htons(PortNumber);
	/*get the server IP*/
	sock->dest_addr.sin_addr.s_addr = inet_addr(PeerName);
	if (sock->dest_addr.sin_addr.s_addr==INADDR_NONE) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Solving %s address\n", PeerName));
		Host = gethostbyname(PeerName);
		if (Host == NULL) {
			switch (LASTSOCKERROR) {
#ifndef __SYMBIAN32__
			case ENETDOWN:
				return GF_IP_NETWORK_FAILURE;
				//case ENOHOST: return GF_IP_ADDRESS_NOT_FOUND;
#endif
			default:
				return GF_IP_NETWORK_FAILURE;
			}
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Host %s found\n", PeerName));
		memcpy((char *) &sock->dest_addr.sin_addr, Host->h_addr_list[0], sizeof(u32));
	}

	if (sock->flags & GF_SOCK_IS_TCP) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Connecting to %s:%d\n", PeerName, PortNumber));
		ret = connect(sock->socket, (struct sockaddr *) &sock->dest_addr, sizeof(struct sockaddr));
		if (ret == SOCKET_ERROR) {
			u32 res = LASTSOCKERROR;
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Sock_IPV4] Couldn't connect socket - last sock error %d\n", res));
			switch (res) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
#ifdef WIN32
			case WSAEINVAL:
				if (sock->flags & GF_SOCK_NON_BLOCKING)
					return GF_IP_SOCK_WOULD_BLOCK;
#endif
			case EISCONN:
				return GF_OK;
			case ENOTCONN:
				return GF_IP_CONNECTION_FAILURE;
			case ECONNRESET:
				return GF_IP_CONNECTION_FAILURE;
			case EMSGSIZE:
				return GF_IP_CONNECTION_FAILURE;
			case ECONNABORTED:
				return GF_IP_CONNECTION_FAILURE;
			case ENETDOWN:
				return GF_IP_CONNECTION_FAILURE;
			default:
				return GF_IP_CONNECTION_FAILURE;
			}
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Connected to %s:%d\n", PeerName, PortNumber));
	}
#endif
	return GF_OK;
}


//binds the given socket to the specified port. If ReUse is true
//this will enable reuse of ports on a single machine
GF_EXPORT
GF_Err gf_sk_bind(GF_Socket *sock, const char *local_ip, u16 port, const char *peer_name, u16 peer_port, u32 options)
{
#ifdef GPAC_HAS_IPV6
	struct addrinfo *res, *aip;
	int af;
	u32 type;
#else
	u32 ip_add;
	size_t addrlen;
	struct sockaddr_in LocalAdd;
	struct hostent *Host;
#if 0
	char buf[GF_MAX_IP_NAME_LEN];
#endif

#endif
	s32 ret;
	s32 optval;

	if (!sock || sock->socket) return GF_BAD_PARAM;

#ifndef WIN32
	if(!local_ip) {
		if(!peer_name || !strcmp(peer_name,"localhost")) {
			peer_name="127.0.0.1";
		}
	}
#endif

#ifdef GPAC_HAS_IPV6
	type = (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM;
	af = (options & GF_SOCK_FORCE_IPV6) ? PF_INET6 : PF_UNSPEC;
	if (!gf_net_has_ipv6()) af = PF_INET;
	/*probe way to peer: is it V4 or V6? */
	if (peer_name && peer_port) {
		res = gf_sk_get_ipv6_addr(peer_name, peer_port, af, AI_PASSIVE, type);
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Socket] Cannot get IPV6 host name for %s:%d\n", peer_name, peer_port));
			return GF_IP_ADDRESS_NOT_FOUND;
		}
#ifdef WIN32
		/*win32 has troubles redirecting IPV4 datagrams to IPV6 sockets, so override
		local family type to avoid IPV4(S)->IPV6(C) UDP*/
		af = res->ai_family;
#endif
		memcpy(&sock->dest_addr, res->ai_addr, res->ai_addrlen);
		sock->dest_addr_len = (u32) res->ai_addrlen;
		freeaddrinfo(res);
	}

	/*turn on MobileIP*/
	if (local_ip && MobileIPAdd && !strcmp(MobileIPAdd, local_ip) ) {
		if (gf_net_mobileip_ctrl(GF_TRUE)==GF_OK) {
			sock->flags |= GF_SOCK_IS_MIP;
		} else {
			/*res = */gf_sk_get_ipv6_addr(NULL, port, af, AI_PASSIVE, type);
			local_ip = NULL;
		}
	}
	res = gf_sk_get_ipv6_addr(local_ip, port, af, AI_PASSIVE, type);
	if (!res) {
		if (local_ip) {
			res = gf_sk_get_ipv6_addr(NULL, port, af, AI_PASSIVE, type);
			local_ip = NULL;
		}
		if (!res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Socket] Cannot get IPV6 host name for %s:%d\n", local_ip, port));
			return GF_IP_ADDRESS_NOT_FOUND;
		}
	}

	/*for all interfaces*/
	for (aip=res; aip!=NULL; aip=aip->ai_next) {
		if (type != (u32) aip->ai_socktype) continue;

		if (aip->ai_next && (aip->ai_next->ai_family==PF_INET) && !gf_net_is_ipv6(peer_name)) continue;

		sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
		if (sock->socket == INVALID_SOCKET) {
			sock->socket = NULL_SOCKET;
			continue;
		}
		if (options & GF_SOCK_REUSE_PORT) {
			optval = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));
#ifdef SO_REUSEPORT
			optval = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_REUSEPORT, SSO_CAST &optval, sizeof(optval));
#endif
		}

		if (sock->flags & GF_SOCK_NON_BLOCKING) gf_sk_set_block_mode(sock, GF_TRUE);

		if (peer_name && peer_port)
			sock->flags |= GF_SOCK_HAS_PEER;


		ret = bind(sock->socket, aip->ai_addr, (int) aip->ai_addrlen);
		if (ret == SOCKET_ERROR) {
			closesocket(sock->socket);
			sock->socket = NULL_SOCKET;
			continue;
		}

		if (aip->ai_family==PF_INET6) sock->flags |= GF_SOCK_IS_IPV6;
		else sock->flags &= ~GF_SOCK_IS_IPV6;

		freeaddrinfo(res);
		return GF_OK;
	}
	freeaddrinfo(res);
	GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Socket] Cannot bind to host %s port %d\n", local_ip, port));
	return GF_IP_CONNECTION_FAILURE;

#else

	sock->socket = socket(AF_INET, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock->flags & GF_SOCK_NON_BLOCKING) gf_sk_set_block_mode(sock, 1);
	sock->flags &= ~GF_SOCK_IS_IPV6;

	memset((void *) &LocalAdd, 0, sizeof(LocalAdd));

	/*turn on MobileIP*/
	if (local_ip && MobileIPAdd && !strcmp(MobileIPAdd, local_ip) ) {
		if (gf_net_mobileip_ctrl(1)==GF_OK) {
			sock->flags |= GF_SOCK_IS_MIP;
		} else {
			local_ip = NULL;
		}
	}
	/*setup the address*/
	ip_add = 0;
	if (local_ip) ip_add = inet_addr(local_ip);

	if (!ip_add) {
#if 0
		buf[0] = 0;
		ret = gethostname(buf, GF_MAX_IP_NAME_LEN);
		/*get the IP address*/
		Host = gethostbyname(buf);
		if (Host != NULL) {
			memcpy((char *) &LocalAdd.sin_addr, Host->h_addr_list[0], sizeof(LocalAdd.sin_addr));
			ip_add = LocalAdd.sin_addr.s_addr;
		} else {
			ip_add = INADDR_ANY;
		}
#else
		ip_add = INADDR_ANY;
#endif
	}
	if (peer_name && peer_port) {
#ifdef WIN32
		if ((inet_addr(peer_name)== ip_add)) {
			optval = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_USELOOPBACK, SSO_CAST &optval, sizeof(optval));
		}
#endif
	}

	LocalAdd.sin_family = AF_INET;
	LocalAdd.sin_port = htons(port);
	LocalAdd.sin_addr.s_addr = ip_add;
	addrlen = sizeof(struct sockaddr_in);


	if (options & GF_SOCK_REUSE_PORT) {
		optval = 1;
		setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, SSO_CAST &optval, sizeof(optval));
#ifdef SO_REUSEPORT
		optval = 1;
		setsockopt(sock->socket, SOL_SOCKET, SO_REUSEPORT, SSO_CAST &optval, sizeof(optval));
#endif
	}

	/*bind the socket*/
	ret = bind(sock->socket, (struct sockaddr *) &LocalAdd, (int) addrlen);
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] cannot bind socket - socket error %x\n", LASTSOCKERROR));
		ret = GF_IP_CONNECTION_FAILURE;
	}

	if (peer_name && peer_port) {
		sock->dest_addr.sin_port = htons(peer_port);
		sock->dest_addr.sin_family = AF_INET;
		sock->dest_addr.sin_addr.s_addr = inet_addr(peer_name);
		if (sock->dest_addr.sin_addr.s_addr == INADDR_NONE) {
			Host = gethostbyname(peer_name);
			if (Host == NULL) ret = GF_IP_ADDRESS_NOT_FOUND;
			else memcpy((char *) &sock->dest_addr.sin_addr, Host->h_addr_list[0], sizeof(u32));
		}
		sock->flags |= GF_SOCK_HAS_PEER;
	}
	if (sock->flags & GF_SOCK_HAS_PEER) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[socket] socket bound to %08X - port %d - remote peer: %s:%d\n", ip_add, port, peer_name, peer_port));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[socket] socket bound to %08X - port %d\n", ip_add, port));
	}
	return ret;
#endif
}

//send length bytes of a buffer
GF_EXPORT
GF_Err gf_sk_send(GF_Socket *sock, const char *buffer, u32 length)
{
	u32 count;
	s32 res;
	Bool not_ready = GF_FALSE;
#ifndef __SYMBIAN32__
	int ready;
	struct timeval timeout;
	fd_set Group;
#endif

	//the socket must be bound or connected
	if (!sock || !sock->socket) return GF_BAD_PARAM;

#ifndef __SYMBIAN32__
	//can we write?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = 0;
	timeout.tv_usec = sock->usec_wait;

	//TODO CHECK IF THIS IS CORRECT
	ready = select((int) sock->socket+1, NULL, &Group, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}

	//should never happen (to check: is writeability is guaranteed for not-connected sockets)
	if (!ready || !FD_ISSET(sock->socket, &Group)) {
		not_ready = GF_TRUE;
	}
#endif

	//direct writing
	count = 0;
	while (count < length) {
		if (sock->flags & GF_SOCK_HAS_PEER) {
			res = (s32) sendto(sock->socket, (char *) buffer+count,  length - count, 0, (struct sockaddr *) &sock->dest_addr, sock->dest_addr_len);
		} else {
			res = (s32) send(sock->socket, (char *) buffer+count, length - count, 0);
		}
		if (res == SOCKET_ERROR) {
			if (not_ready)
				return GF_IP_NETWORK_EMPTY;

			switch (res = LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
#ifndef __SYMBIAN32__
			case ENOTCONN:
			case ECONNRESET:
				return GF_IP_CONNECTION_CLOSED;
#endif
			default:
				return GF_IP_NETWORK_FAILURE;
			}
		}
		count += res;
	}
	return GF_OK;
}


GF_EXPORT
u32 gf_sk_is_multicast_address(const char *multi_IPAdd)
{
#ifdef GPAC_HAS_IPV6
	u32 val;
	char *sep;
	struct addrinfo *res;
	if (!multi_IPAdd) return 0;
	/*IPV6 multicast address*/
	sep = strchr(multi_IPAdd, ':');
	if (sep) sep = strchr(multi_IPAdd, ':');
	if (sep && !strnicmp(multi_IPAdd, "ff", 2)) return 1;
	/*ipv4 multicast address*/
	res = gf_sk_get_ipv6_addr((char*)multi_IPAdd, 7000, AF_UNSPEC, AI_PASSIVE, SOCK_DGRAM);
	if (!res) return 0;
	val = 0;
	if (res->ai_addr->sa_family == AF_INET) {
		val = IN_MULTICAST(ntohl(((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr));
	} else if (res->ai_addr->sa_family == AF_INET6) {
		val = IN6_IS_ADDR_MULTICAST(& ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr);
	}
	freeaddrinfo(res);
	return val;
#else
	if (!multi_IPAdd) return 0;
	return ((htonl(inet_addr(multi_IPAdd)) >> 8) & 0x00f00000) == 0x00e00000;
#endif
}

GF_EXPORT
GF_Err gf_sk_setup_multicast(GF_Socket *sock, const char *multi_IPAdd, u16 MultiPortNumber, u32 TTL, Bool NoBind, char *local_interface_ip)
{
	s32 ret;
	u32 flag;
	struct ip_mreq M_req;
	u32 optval;
#ifdef GPAC_HAS_IPV6
	struct sockaddr *addr;
	struct addrinfo *res, *aip;
	Bool is_ipv6 = GF_FALSE;
	u32 type;
#endif
	u32 local_add_id;

	if (!sock || sock->socket) return GF_BAD_PARAM;

	if (TTL > 255) TTL = 255;

	/*check the address*/
	if (!gf_sk_is_multicast_address(multi_IPAdd)) return GF_BAD_PARAM;

	/*turn on MobileIP*/
	if (local_interface_ip && MobileIPAdd && !strcmp(MobileIPAdd, local_interface_ip) ) {
		if (gf_net_mobileip_ctrl(GF_TRUE)==GF_OK) {
			sock->flags |= GF_SOCK_IS_MIP;
		} else {
			local_interface_ip = NULL;
		}
	}


#ifdef GPAC_HAS_IPV6
	is_ipv6 = gf_net_is_ipv6(multi_IPAdd) || gf_net_is_ipv6(local_interface_ip) ? GF_TRUE : GF_FALSE;
	type = (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM;

	if (is_ipv6) {

		res = gf_sk_get_ipv6_addr(local_interface_ip, MultiPortNumber, AF_UNSPEC, AI_PASSIVE, type);
		if (!res) {
			if (local_interface_ip) {
				res = gf_sk_get_ipv6_addr(NULL, MultiPortNumber, AF_UNSPEC, AI_PASSIVE, type);
				local_interface_ip = NULL;
			}
			if (!res) return GF_IP_CONNECTION_FAILURE;
		}

		/*for all interfaces*/
		for (aip=res; aip!=NULL; aip=aip->ai_next) {
			if (type != (u32) aip->ai_socktype) continue;
			sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
			if (sock->socket == INVALID_SOCKET) {
				sock->socket = NULL_SOCKET;
				continue;
			}

			if ((aip->ai_family!=PF_INET) && aip->ai_next && (aip->ai_next->ai_family==PF_INET) && !gf_net_is_ipv6(multi_IPAdd)) continue;

			/*enable address reuse*/
			optval = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));
#ifdef SO_REUSEPORT
			optval = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_REUSEPORT, SSO_CAST &optval, sizeof(optval));
#endif

			/*TODO: copy over other properties (recption buffer size & co)*/
			if (sock->flags & GF_SOCK_NON_BLOCKING) gf_sk_set_block_mode(sock, GF_TRUE);

			memcpy(&sock->dest_addr, aip->ai_addr, aip->ai_addrlen);
			sock->dest_addr_len = (u32) aip->ai_addrlen;

			if (!NoBind) {
				ret = bind(sock->socket, aip->ai_addr, (int) aip->ai_addrlen);
				if (ret == SOCKET_ERROR) {
					closesocket(sock->socket);
					sock->socket = NULL_SOCKET;
					continue;
				}
			}
			if (aip->ai_family==PF_INET6) sock->flags |= GF_SOCK_IS_IPV6;
			else sock->flags &= ~GF_SOCK_IS_IPV6;
			break;
		}
		freeaddrinfo(res);
		if (!sock->socket) return GF_IP_CONNECTION_FAILURE;


		if (!gf_sk_ipv6_set_remote_address(sock, multi_IPAdd, MultiPortNumber))
			return GF_IP_CONNECTION_FAILURE;

		addr = (struct sockaddr *)&sock->dest_addr;
		if (addr->sa_family == AF_INET) {
			M_req.imr_multiaddr.s_addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
			M_req.imr_interface.s_addr = INADDR_ANY;
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &M_req, sizeof(M_req));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
			/*set TTL*/
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &TTL, sizeof(TTL));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
			/*Disable loopback*/
			flag = 1;
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &flag, sizeof(flag));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		}

		if (addr->sa_family == AF_INET6) {
			struct ipv6_mreq M_reqV6;

			memcpy(&M_reqV6.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)addr)->sin6_addr), sizeof(struct in6_addr));
			M_reqV6.ipv6mr_interface = 0;

			/*set TTL*/
			ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &TTL, sizeof(TTL));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
			/*enable loopback*/
			flag = 1;
			ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &flag, sizeof(flag));
			if (ret == SOCKET_ERROR) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Socket] Cannot disale multicast loop: error %d\n", LASTSOCKERROR));
			}

			ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *) &M_reqV6, sizeof(M_reqV6));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		}
		sock->flags |= GF_SOCK_IS_MULTICAST | GF_SOCK_HAS_PEER;
		return GF_OK;
	}
#endif

	//IPv4 setup
	sock->socket = socket(AF_INET, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock->flags & GF_SOCK_NON_BLOCKING) gf_sk_set_block_mode(sock, GF_TRUE);
	sock->flags &= ~GF_SOCK_IS_IPV6;

	/*enable address reuse*/
	optval = 1;
	ret = setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, SSO_CAST &optval, sizeof(optval));
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[core] Failed to set SO_REUSEADDR: error %d\n", LASTSOCKERROR));
	}
#ifdef SO_REUSEPORT
	optval = 1;
	ret = setsockopt(sock->socket, SOL_SOCKET, SO_REUSEPORT, SSO_CAST &optval, sizeof(optval));
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[core] Failed to set SO_REUSEPORT: error %d\n", LASTSOCKERROR));
	}
#endif

	if (local_interface_ip) local_add_id = inet_addr(local_interface_ip);
	else local_add_id = htonl(INADDR_ANY);

	if (!NoBind) {
		struct sockaddr_in local_address;

		memset(&local_address, 0, sizeof(struct sockaddr_in ));
		local_address.sin_family = AF_INET;
//		local_address.sin_addr.s_addr = local_add_id;
		local_address.sin_addr.s_addr = htonl(INADDR_ANY);
		local_address.sin_port = htons( MultiPortNumber);

		ret = bind(sock->socket, (struct sockaddr *) &local_address, sizeof(local_address));
		if (ret == SOCKET_ERROR) {
			/*retry without specifying the local add*/
			local_address.sin_addr.s_addr = local_add_id = htonl(INADDR_ANY);
			local_interface_ip = NULL;
			ret = bind(sock->socket, (struct sockaddr *) &local_address, sizeof(local_address));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		}
		/*setup local interface*/
		if (local_interface_ip) {
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_IF, (void *) &local_add_id, sizeof(local_add_id));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		}
	}

	/*now join the multicast*/
	M_req.imr_multiaddr.s_addr = inet_addr(multi_IPAdd);
	M_req.imr_interface.s_addr = local_add_id;

	ret = setsockopt(sock->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &M_req, sizeof(M_req));
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] cannot join multicast: error %d\n", LASTSOCKERROR));
		return GF_IP_CONNECTION_FAILURE;
	}
	/*set the Time To Live*/
	if (TTL) {
		ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&TTL, sizeof(TTL));
		if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
	}

	/*enable loopback*/
	flag = 1;
	ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &flag, sizeof(flag));
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Socket] Cannot disale multicast loop: error %d\n", LASTSOCKERROR));
	}

#ifdef GPAC_HAS_IPV6
	((struct sockaddr_in *) &sock->dest_addr)->sin_family = AF_INET;
	((struct sockaddr_in *) &sock->dest_addr)->sin_addr.s_addr = M_req.imr_multiaddr.s_addr;
	((struct sockaddr_in *) &sock->dest_addr)->sin_port = htons( MultiPortNumber);
	sock->dest_addr_len = sizeof(struct sockaddr);
#else
	sock->dest_addr.sin_family = AF_INET;
	sock->dest_addr.sin_addr.s_addr = M_req.imr_multiaddr.s_addr;
	sock->dest_addr.sin_port = htons( MultiPortNumber);
#endif

	sock->flags |= GF_SOCK_IS_MULTICAST | GF_SOCK_HAS_PEER;
	return GF_OK;
}

#include <gpac/list.h>
struct __tag_sock_group
{
	GF_List *sockets;
	fd_set group;
};

GF_SockGroup *gf_sk_group_new()
{
	GF_SockGroup *tmp;
	GF_SAFEALLOC(tmp, GF_SockGroup);
	tmp->sockets = gf_list_new();
	FD_ZERO(&tmp->group);
	return tmp;
}

void gf_sk_group_del(GF_SockGroup *sg)
{
	gf_list_del(sg->sockets);
	gf_free(sg);
}

void gf_sk_group_register(GF_SockGroup *sg, GF_Socket *sk)
{
	if (sg && sk) {
		gf_list_add(sg->sockets, sk);
	}
}
void gf_sk_group_unregister(GF_SockGroup *sg, GF_Socket *sk)
{
	if (sg && sk) {
		gf_list_del_item(sg->sockets, sk);
	}
}

GF_Err gf_sk_group_select(GF_SockGroup *sg, u32 usec_wait)
{
	s32 ready;
	u32 i=0;
	struct timeval timeout;
	u32 max_fd=0;
	GF_Socket *sock;

	FD_ZERO(&sg->group);
	while ((sock = gf_list_enum(sg->sockets, &i))) {
		FD_SET(sock->socket, &sg->group);
		if (max_fd < (u32) sock->socket) max_fd = (u32) sock->socket;
	}
	if (usec_wait>=1000000) {
		timeout.tv_sec = usec_wait/1000000;
		timeout.tv_usec = (u32) (usec_wait - (timeout.tv_sec*1000000));
	} else {
		timeout.tv_sec = 0;
		timeout.tv_usec = usec_wait;
	}
	ready = select((int) max_fd+1, &sg->group, NULL, NULL, &timeout);

	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EBADF:
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot select, BAD descriptor\n"));
			return GF_IP_CONNECTION_CLOSED;
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		case EINTR:
			/* Interrupted system call, not really important... */
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] network is lost\n"));
			return GF_IP_NETWORK_EMPTY;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot select (error %d)\n", LASTSOCKERROR));
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!ready) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[socket] nothing to be read - ready %d\n", ready));
		return GF_IP_NETWORK_EMPTY;
	}
	return GF_OK;
}

Bool gf_sk_group_sock_is_set(GF_SockGroup *sg, GF_Socket *sk)
{
	if (sg && sk && FD_ISSET(sk->socket, &sg->group)) return GF_TRUE;
	return GF_FALSE;
}


//fetch nb bytes on a socket and fill the buffer from startFrom
//length is the allocated size of the receiving buffer
//BytesRead is the number of bytes read from the network
GF_Err gf_sk_receive_internal(GF_Socket *sock, char *buffer, u32 length, u32 startFrom, u32 *BytesRead, Bool do_select)
{
	s32 res;
#ifndef __SYMBIAN32__
	s32 ready;
	struct timeval timeout;
	fd_set Group;
#endif

	*BytesRead = 0;
	if (!sock || !sock->socket) return GF_BAD_PARAM;
	if (startFrom >= length) return GF_IO_ERR;

#ifndef __SYMBIAN32__
	if (do_select) {
		//can we read?
		timeout.tv_sec = 0;
		timeout.tv_usec = sock->usec_wait;
		FD_ZERO(&Group);
		FD_SET(sock->socket, &Group);
		ready = select((int) sock->socket+1, &Group, NULL, NULL, &timeout);

		if (ready == SOCKET_ERROR) {
			switch (LASTSOCKERROR) {
			case EBADF:
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot select, BAD descriptor\n"));
				return GF_IP_CONNECTION_CLOSED;
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
			case EINTR:
				/* Interrupted system call, not really important... */
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] network is lost\n"));
				return GF_IP_NETWORK_EMPTY;
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot select (error %d)\n", LASTSOCKERROR));
				return GF_IP_NETWORK_FAILURE;
			}
		}
		if (!ready || !FD_ISSET(sock->socket, &Group)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[socket] nothing to be read - ready %d\n", ready));
			return GF_IP_NETWORK_EMPTY;
		}
	}
#endif
	if (sock->flags & GF_SOCK_HAS_PEER)
		res = (s32) recvfrom(sock->socket, (char *) buffer + startFrom, length - startFrom, 0, (struct sockaddr *)&sock->dest_addr, &sock->dest_addr_len);
	else {
		res = (s32) recv(sock->socket, (char *) buffer + startFrom, length - startFrom, 0);
		if (res == 0)
			return GF_IP_CONNECTION_CLOSED;
	}

	if (res == SOCKET_ERROR) {
		res = LASTSOCKERROR;
		switch (res) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
#ifndef __SYMBIAN32__
		case EMSGSIZE:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] error reading - socket error %d\n",  res));
			return GF_OUT_OF_MEM;
		case ENOTCONN:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] error reading - not connected\n"));
			return GF_IP_CONNECTION_CLOSED;
		case ECONNRESET:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] error reading - connection reset\n"));
			return GF_IP_CONNECTION_CLOSED;
		case ECONNABORTED:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] error reading - connection aborted\n"));
			return GF_IP_CONNECTION_CLOSED;
#endif
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] error reading - socket error %d\n",  res));
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!res) return GF_IP_NETWORK_EMPTY;
	*BytesRead = res;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_receive(GF_Socket *sock, char *buffer, u32 length, u32 startFrom, u32 *BytesRead)
{
	return gf_sk_receive_internal(sock, buffer, length, startFrom, BytesRead, GF_TRUE);
}

GF_EXPORT
GF_Err gf_sk_receive_no_select(GF_Socket *sock, char *buffer, u32 length, u32 startFrom, u32 *BytesRead)
{
	return gf_sk_receive_internal(sock, buffer, length, startFrom, BytesRead, GF_FALSE);
}

GF_EXPORT
GF_Err gf_sk_listen(GF_Socket *sock, u32 MaxConnection)
{
	s32 i;
	if (!sock || !sock->socket) return GF_BAD_PARAM;
	if (MaxConnection >= SOMAXCONN) MaxConnection = SOMAXCONN;
	i = listen(sock->socket, MaxConnection);
	if (i == SOCKET_ERROR) return GF_IP_NETWORK_FAILURE;
	sock->flags |= GF_SOCK_IS_LISTENING;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_accept(GF_Socket *sock, GF_Socket **newConnection)
{
	u32 client_address_size;
	SOCKET sk;
#ifndef __SYMBIAN32__
	s32 ready;
	struct timeval timeout;
	fd_set Group;
#endif
	*newConnection = NULL;
	if (!sock || !(sock->flags & GF_SOCK_IS_LISTENING) ) return GF_BAD_PARAM;

#ifndef __SYMBIAN32__
	//can we read?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = 0;
	timeout.tv_usec = sock->usec_wait;

	//TODO - check if this is correct
	ready = select((int) sock->socket+1, &Group, NULL, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!ready || !FD_ISSET(sock->socket, &Group)) return GF_IP_NETWORK_EMPTY;
#endif

#ifdef GPAC_HAS_IPV6
	client_address_size = sizeof(struct sockaddr_in6);
#else
	client_address_size = sizeof(struct sockaddr_in);
#endif
	sk = accept(sock->socket, (struct sockaddr *) &sock->dest_addr, &client_address_size);

	//we either have an error or we have no connections
	if (sk == INVALID_SOCKET) {
//		if (sock->flags & GF_SOCK_NON_BLOCKING) return GF_IP_NETWORK_FAILURE;
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}

	(*newConnection) = (GF_Socket *) gf_malloc(sizeof(GF_Socket));
	(*newConnection)->socket = sk;
	(*newConnection)->flags = sock->flags & ~GF_SOCK_IS_LISTENING;
#ifdef GPAC_HAS_IPV6
	memcpy( &(*newConnection)->dest_addr, &sock->dest_addr, client_address_size);
	memset(&sock->dest_addr, 0, sizeof(struct sockaddr_in6));
#else
	memcpy( &(*newConnection)->dest_addr, &sock->dest_addr, client_address_size);
	memset(&sock->dest_addr, 0, sizeof(struct sockaddr_in));
#endif

#if defined(WIN32) || defined(_WIN32_WCE)
	wsa_init++;
#endif

	(*newConnection)->dest_addr_len = client_address_size;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_get_local_info(GF_Socket *sock, u16 *Port, u32 *Familly)
{
#ifdef GPAC_HAS_IPV6
	struct sockaddr_in6 the_add;
#else
	struct sockaddr_in the_add;
#endif
	u32 size;

	if (!sock || !sock->socket) return GF_BAD_PARAM;

	if (Port) {
#ifdef GPAC_HAS_IPV6
		size = sizeof(struct sockaddr_in6);
		if (getsockname(sock->socket, (struct sockaddr *) &the_add, &size) == SOCKET_ERROR) return GF_IP_NETWORK_FAILURE;
		*Port = (u32) ntohs(the_add.sin6_port);
#else
		size = sizeof(struct sockaddr_in);
		if (getsockname(sock->socket, (struct sockaddr *) &the_add, &size) == SOCKET_ERROR) return GF_IP_NETWORK_FAILURE;
		*Port = (u32) ntohs(the_add.sin_port);
#endif
	}
	if (Familly) {
		/*		size = 4;
				if (getsockopt(sock->socket, SOL_SOCKET, SO_TYPE, (char *) &fam, &size) == SOCKET_ERROR)
					return GF_IP_NETWORK_FAILURE;
				*Familly = fam;
		*/
		if (sock->flags & GF_SOCK_IS_TCP) *Familly = GF_SOCK_TYPE_TCP;
		else  *Familly = GF_SOCK_TYPE_UDP;
	}
	return GF_OK;
}

//we have to do this for the server sockets as we use only one thread
GF_EXPORT
GF_Err gf_sk_server_mode(GF_Socket *sock, Bool serverOn)
{
	u32 one;

	if (!sock || !(sock->flags & GF_SOCK_IS_TCP) || !sock->socket)
		return GF_BAD_PARAM;

	one = serverOn ? 1 : 0;
	setsockopt(sock->socket, IPPROTO_TCP, TCP_NODELAY, SSO_CAST &one, sizeof(u32));
#ifndef __SYMBIAN32__
	setsockopt(sock->socket, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(u32));
#endif
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_get_remote_address(GF_Socket *sock, char *buf)
{
#ifdef GPAC_HAS_IPV6
	char clienthost[NI_MAXHOST];
	struct sockaddr_in6 * addrptr = (struct sockaddr_in6 *)(&sock->dest_addr_len);
	if (!sock || sock->socket) return GF_BAD_PARAM;
	if (getnameinfo((struct sockaddr *)addrptr, sock->dest_addr_len, clienthost, sizeof(clienthost), NULL, 0, NI_NUMERICHOST))
		return GF_IP_ADDRESS_NOT_FOUND;
	strcpy(buf, clienthost);
#else
	if (!sock || !sock->socket) return GF_BAD_PARAM;
	strcpy(buf, inet_ntoa(sock->dest_addr.sin_addr));
#endif
	return GF_OK;
}




//send length bytes of a buffer
GF_EXPORT
GF_Err gf_sk_send_to(GF_Socket *sock, const char *buffer, u32 length, char *remoteHost, u16 remotePort)
{
	u32 count, remote_add_len;
	s32 res;
#ifdef GPAC_HAS_IPV6
	struct sockaddr_storage remote_add;
#else
	struct sockaddr_in remote_add;
	struct hostent *Host;
#endif
#ifndef __SYMBIAN32__
	s32 ready;
	struct timeval timeout;
	fd_set Group;
#endif

	//the socket must be bound or connected
	if (!sock || !sock->socket) return GF_BAD_PARAM;
	if (remoteHost && !remotePort) return GF_BAD_PARAM;

#ifndef __SYMBIAN32__
	//can we write?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = 0;
	timeout.tv_usec = sock->usec_wait;

	//TODO - check if this is correct
	ready = select((int) sock->socket+1, NULL, &Group, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!ready || !FD_ISSET(sock->socket, &Group)) return GF_IP_NETWORK_EMPTY;
#endif


	/*setup the address*/
#ifdef GPAC_HAS_IPV6
	remote_add.ss_family = AF_INET6;
	//if a remote host is specified, use it. Otherwise use the default host
	if (remoteHost) {
		//setup the address
		struct addrinfo *res = gf_sk_get_ipv6_addr(remoteHost, remotePort, AF_UNSPEC, 0, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM);
		if (!res) return GF_IP_ADDRESS_NOT_FOUND;
		memcpy(&remote_add, res->ai_addr, res->ai_addrlen);
		remote_add_len = (u32) res->ai_addrlen;
		freeaddrinfo(res);
	} else {
		struct sockaddr_in6 *remotePtr = (struct sockaddr_in6 *)&remote_add;
		struct sockaddr_in6 * addrptr = (struct sockaddr_in6 *)(&sock->dest_addr);
		remotePtr->sin6_port = addrptr->sin6_port;
		remotePtr->sin6_addr = addrptr->sin6_addr;
		remote_add_len = sock->dest_addr_len;
	}
#else
	remote_add_len = sizeof(struct sockaddr_in);
	remote_add.sin_family = AF_INET;
	//if a remote host is specified, use it. Otherwise use the default host
	if (remoteHost) {
		//setup the address
		remote_add.sin_port = htons(remotePort);
		//get the server IP
		Host = gethostbyname(remoteHost);
		if (Host == NULL) return GF_IP_ADDRESS_NOT_FOUND;
		memcpy((char *) &remote_add.sin_addr, Host->h_addr_list[0], sizeof(u32));
	} else {
		remote_add.sin_port = sock->dest_addr.sin_port;
		remote_add.sin_addr.s_addr = sock->dest_addr.sin_addr.s_addr;
	}
#endif
	count = 0;
	while (count < length) {
		res = (s32) sendto(sock->socket, (char *) buffer+count, length - count, 0, (struct sockaddr *) &remote_add, remote_add_len);
		if (res == SOCKET_ERROR) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
			default:
				return GF_IP_NETWORK_FAILURE;
			}
		}
		count += res;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_receive_wait(GF_Socket *sock, char *buffer, u32 length, u32 startFrom, u32 *BytesRead, u32 Second )
{
	s32 res;
#ifndef __SYMBIAN32__
	s32 ready;
	struct timeval timeout;
	fd_set Group;
#endif

	*BytesRead = 0;
	if (startFrom >= length) return GF_OK;

#ifndef __SYMBIAN32__
	//can we read?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = Second;
	timeout.tv_usec = sock->usec_wait;

	ready = select((int) sock->socket+1, &Group, NULL, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!FD_ISSET(sock->socket, &Group)) {
		return GF_IP_NETWORK_EMPTY;
	}
#endif


	res = (s32) recv(sock->socket, (char *) buffer + startFrom, length - startFrom, 0);
	if (res == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}
	*BytesRead = res;
	return GF_OK;
}


//send length bytes of a buffer
GF_EXPORT
GF_Err gf_sk_send_wait(GF_Socket *sock, const char *buffer, u32 length, u32 Second )
{
	u32 count;
	s32 res;
#ifndef __SYMBIAN32__
	s32 ready;
	struct timeval timeout;
	fd_set Group;
#endif

	//the socket must be bound or connected
	if (!sock || !sock->socket) return GF_BAD_PARAM;

#ifndef __SYMBIAN32__
	//can we write?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = Second;
	timeout.tv_usec = sock->usec_wait;

	//TODO - check if this is correct
	ready = select((int) sock->socket+1, NULL, &Group, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}
	//should never happen (to check: is writeability is guaranteed for not-connected sockets)
	if (!ready || !FD_ISSET(sock->socket, &Group)) {
		return GF_IP_NETWORK_EMPTY;
	}
#endif

	//direct writing
	count = 0;
	while (count < length) {
		res = (s32) send(sock->socket, (char *) buffer+count, length - count, 0);
		if (res == SOCKET_ERROR) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
#ifndef __SYMBIAN32__
			case ECONNRESET:
				return GF_IP_CONNECTION_CLOSED;
#endif
			default:
				return GF_IP_NETWORK_FAILURE;
			}
		}
		count += res;
	}
	return GF_OK;
}

GF_EXPORT
u32 gf_htonl(u32 val)
{
	return htonl(val);
}


GF_EXPORT
u32 gf_ntohl(u32 val)
{
	return htonl(val);
}

GF_EXPORT
u16 gf_htons(u16 val)
{
	return htons(val);
}


GF_EXPORT
u16 gf_tohs(u16 val)
{
	return htons(val);
}

#endif /*GPAC_DISABLE_CORE_TOOLS*/
