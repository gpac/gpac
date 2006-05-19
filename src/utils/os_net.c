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


#include <gpac/network.h>


#if defined(WIN32) || defined(_WIN32_WCE)

#ifdef _WIN32_WCE
#include <winsock.h>
#else
#include <sys/timeb.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <windows.h>

#ifdef IPV6_MULTICAST_IF
#define GPAC_IPV6 1
#ifndef __GNUC__
#pragma message("Using WinSock IPV6")
#endif
#else
#undef GPAC_IPV6
#ifndef __GNUC__
#pragma message("Using WinSock IPV4")
#endif
#endif

/*common win32 redefs*/
#define EAGAIN				WSAEWOULDBLOCK
#define EISCONN				WSAEISCONN
#define ENOTCONN			WSAENOTCONN
#define ECONNRESET			WSAECONNRESET
#define EMSGSIZE			WSAEMSGSIZE
#define ECONNABORTED		WSAECONNABORTED
#define ENETDOWN			WSAENETDOWN
#define	ENOHOST				WSAHOST_NOT_FOUND

#define LASTSOCKERROR WSAGetLastError()

/*the number of sockets used. This because the WinSock lib needs init*/
static u32 wsa_init = 0;

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
#include <arpa/inet.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define LASTSOCKERROR errno

typedef s32 SOCKET;
#define closesocket(v) close(v)

#endif


#define SOCK_MICROSEC_WAIT	500

/* socket status */
#define SK_STATUS_CREATE	0x01
#define SK_STATUS_BIND		0x02
#define SK_STATUS_CONNECT	0x03
#define SK_STATUS_LISTEN	0x04

static u32 ipv6_check_state = 0;


struct __tag_socket
{
	u32 status;
	SOCKET socket;
	u32 type;
	Bool blocking, is_ipv6;
	//this is used for server sockets / multicast sockets
#ifdef GPAC_IPV6
	struct sockaddr_storage dest_addr;
#else
	struct sockaddr_in dest_addr;
#endif
	u32 dest_addr_len;
};

/*
		Some NTP tools
*/

void gf_net_get_ntp(u32 *sec, u32 *frac)
{
	struct timeval now;
#ifdef WIN32
	s32 gettimeofday(struct timeval *tp, void *tz);
#endif
	gettimeofday(&now, NULL);
	*sec = (u32) (now.tv_sec) + GF_NTP_SEC_1900_TO_1970;
	*frac = (u32) ( (now.tv_usec << 12) + (now.tv_usec << 8) - ((now.tv_usec * 3650) >> 6) );
}

u32 gf_net_has_ipv6()
{
#ifdef GPAC_IPV6
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
		s = socket(PF_INET6, SOCK_STREAM, 0);
		if (!wsa_init) WSACleanup();
#else
		s = socket(PF_INET6, SOCK_STREAM, 0);
#endif
		if (!s) ipv6_check_state = 1;
		else {
			ipv6_check_state = 2;
			closesocket(s);
		}
	} 
	return (ipv6_check_state==2);
#else
	return 0;
#endif
}

#ifdef GPAC_IPV6
static struct addrinfo *gf_sk_get_ipv6_addr(char *PeerName, u16 PortNumber, int family, int flags, int sock_type)
{
	struct	addrinfo *res=NULL;
	struct	addrinfo hints;
	char node[50], portstring[20], *service, *dest;

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
 		strcpy(node, PeerName);
 		if (node[0]=='[') {
 			node[strlen(node)-1] = 0;
 			strcpy(node, &node[1]);
 		}
		dest = (char *) node;
	}
	if (getaddrinfo((const char *)dest, (const char *)service, &hints, &res) != 0) return NULL;
	return res;
}

static Bool gf_sk_ipv6_set_remote_address(GF_Socket *sock, char *address, u16 PortNumber)
{
	struct addrinfo *res = gf_sk_get_ipv6_addr(address, PortNumber, AF_UNSPEC, 0, sock->type);
	if (!res) return 0;
    memcpy(&sock->dest_addr, res->ai_addr, res->ai_addrlen);
	sock->dest_addr_len = res->ai_addrlen;
	freeaddrinfo(res);
	return 1;
}
#endif


GF_Err gf_sk_get_host_name(char *buffer)
{
	s32 ret = gethostname(buffer, GF_MAX_IP_NAME_LEN);
	return (ret == SOCKET_ERROR) ? GF_IP_ADDRESS_NOT_FOUND : GF_OK;
}

GF_Err gf_sk_get_local_ip(GF_Socket *sock, char *buffer)
{
#ifdef GPAC_IPV6
	char clienthost[NI_MAXHOST];
    struct sockaddr_storage clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    if (getsockname(sock->socket, (struct sockaddr *)&clientaddr, &addrlen)) return GF_IP_NETWORK_FAILURE;

    if (getnameinfo((struct sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), NULL, 0, NI_NUMERICHOST)) 
		return GF_IP_NETWORK_FAILURE;
	strcpy(buffer, clienthost);
#else
	char *ip;
	struct sockaddr_in name;
	u32 len = sizeof(struct sockaddr_in);
	if (getsockname(sock->socket, (struct sockaddr*) &name, &len)) return GF_IP_NETWORK_FAILURE;
	ip = inet_ntoa(name.sin_addr);
	if (!ip) return GF_IP_NETWORK_FAILURE;
	strcpy(buffer, ip);
#endif
	return GF_OK;
}

GF_Socket *gf_sk_new(u32 SocketType)
{
	GF_Socket *tmp;

	/*init WinSock*/
#ifdef WIN32
	WSADATA Data;
	if (!wsa_init && (WSAStartup(0x0202, &Data)!=0) ) return NULL;
#endif
	if ((SocketType != GF_SOCK_TYPE_UDP) && (SocketType != GF_SOCK_TYPE_TCP)) return NULL;
	GF_SAFEALLOC(tmp, sizeof(GF_Socket) );

	/*always create the socket on V4 IP, switch to V6 is made updon connect*/
	tmp->socket = socket(AF_INET, SocketType, 0);
	if (tmp->socket == INVALID_SOCKET) {
		free(tmp);
#ifdef WIN32
		if (!wsa_init) WSACleanup();
#endif
		return NULL;
	}

	tmp->type = SocketType;
	tmp->status = SK_STATUS_CREATE;
	tmp->blocking = 1;
#ifdef GPAC_IPV6
	memset(&tmp->dest_addr, 0, sizeof(struct sockaddr_storage));
#else
	memset(&tmp->dest_addr, 0, sizeof(struct sockaddr_in));
	tmp->dest_addr_len = sizeof(struct sockaddr);
#endif

#ifdef WIN32
	wsa_init ++;
#endif
	return tmp;
}

GF_Err gf_sk_set_buffer_size(GF_Socket *sock, Bool SendBuffer, u32 NewSize)
{
	if (!sock) return GF_BAD_PARAM;

	if (SendBuffer) {
		setsockopt(sock->socket, SOL_SOCKET, SO_SNDBUF, (char *) &NewSize, sizeof(u32) );
	} else {
		setsockopt(sock->socket, SOL_SOCKET, SO_RCVBUF, (char *) &NewSize, sizeof(u32) );
	}
	return GF_OK;
}

GF_Err gf_sk_set_blocking(GF_Socket *sock, u32 NonBlockingOn)
{
	s32 res;
#ifdef WIN32
	u_long val = NonBlockingOn;
	res = ioctlsocket(sock->socket, FIONBIO, &val);
	if (res) return GF_SERVICE_ERROR;
#else
	s32 flag = fcntl(sock->socket, F_GETFL, 0);
	res = fcntl(sock->socket, F_SETFL, flag | O_NONBLOCK);
	if (res) return GF_SERVICE_ERROR;
	sock->blocking = NonBlockingOn ? 0 : 1;
#endif
	sock->blocking = NonBlockingOn ? 0 : 1;
	return GF_OK;
}


void gf_sk_del(GF_Socket *sock)
{
#ifdef GPAC_IPV6
	/*leave multicast*/
	/* TODO transpose this code for MULTICAST IPV6 
	ip_mreq : use ipv6_mreq in addition for IPv6 support
	INADDR_ANY : use getaddrinfo with nodename=NULL and AI_PASSIVE instead, or use in6addr_any in addition for IPv6 support
	IPPROTO_IP : use IPPROTO_IPV6 in addition for IPv6 support
	IP_DROP_MEMBERSHIP : use IPV6_LEAVE_GROUP in addition for IPv6 support
	if ( ((htonl( sock->RemoteAddress.sin_addr.s_addr ) >> 8) & 0x00f00000) == 0x00e00000) {
		if (sock->status == SK_STATUS_BIND) {
			struct ip_mreq M_req;
			M_req.imr_multiaddr.s_addr = sock->RemoteAddress.sin_addr.s_addr ;
			M_req.imr_interface.s_addr = INADDR_ANY;
			setsockopt(sock->socket, IPPROTO_IPV6, IP_DROP_MEMBERSHIP, (char *) &M_req, sizeof(M_req));
		}
	}
	*/
	closesocket(sock->socket);
#else
	/*leave multicast*/
	if ( ((htonl( sock->dest_addr.sin_addr.s_addr ) >> 8) & 0x00f00000) == 0x00e00000) {
		if (sock->status == SK_STATUS_BIND) {
			struct ip_mreq M_req;
			M_req.imr_multiaddr.s_addr = sock->dest_addr.sin_addr.s_addr ;
			M_req.imr_interface.s_addr = INADDR_ANY;
			setsockopt(sock->socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &M_req, sizeof(M_req));
		}
	}
	closesocket(sock->socket);
#endif

#ifdef WIN32
	wsa_init --;
	if (!wsa_init) WSACleanup();
#endif
	free(sock);
}

void gf_sk_reset(GF_Socket *sock)
{
	u32 clear;
	if (sock) setsockopt(sock->socket, SOL_SOCKET, SO_ERROR, (char *) &clear, sizeof(u32) );
}

s32 gf_sk_get_handle(GF_Socket *sock) 
{
	return sock->socket;
}



//connects a socket to a remote peer on a given port
GF_Err gf_sk_connect(GF_Socket *sock, char *PeerName, u16 PortNumber)
{
	s32 ret;
#ifdef GPAC_IPV6
	struct addrinfo *res, *aip;

	res = gf_sk_get_ipv6_addr(PeerName, PortNumber, AF_UNSPEC, 0, sock->type);
	if (!res) {
/*		switch(error) {
		case EAI_FAMILY: return GF_IP_CONNECTION_FAILURE;
		case EAI_SOCKTYPE:
			return GF_IP_CONNECTION_FAILURE;
		case EAI_BADFLAGS:
			return GF_IP_CONNECTION_FAILURE;
		case EAI_NONAME:
			return GF_IP_CONNECTION_FAILURE;
		case EAI_SERVICE:
			return GF_IP_CONNECTION_FAILURE;
		 case EAI_ADDRFAMILY:
			return GF_IP_CONNECTION_FAILURE;
		default:
			return GF_IP_CONNECTION_FAILURE;
		}
*/
		return GF_IP_CONNECTION_FAILURE;
	}

	/*close socket*/
	closesocket(sock->socket);
	sock->socket = (SOCKET)NULL;

	/*for all interfaces*/
	for (aip=res; aip!=NULL; aip=aip->ai_next) {
		if (sock->type != (u32) aip->ai_socktype) continue;
		sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
		if (sock->socket == INVALID_SOCKET) {
			sock->socket = (SOCKET)NULL;
			continue;
		}
		ret = connect(sock->socket, aip->ai_addr, aip->ai_addrlen);						
		if (ret == SOCKET_ERROR) {
			closesocket(sock->socket);
			sock->socket = (SOCKET)NULL;
			continue;
		}
		/*TODO: copy over other properties (recption buffer size & co)*/
		gf_sk_set_blocking(sock, !sock->blocking);

	    memcpy(&sock->dest_addr, aip->ai_addr, aip->ai_addrlen);
		sock->dest_addr_len = aip->ai_addrlen;
		sock->is_ipv6 = (aip->ai_family==PF_INET6) ? 1 : 0;
		break;
	}
	freeaddrinfo(res);
	if (!sock->socket) return GF_IP_CONNECTION_FAILURE;
	sock->status = SK_STATUS_CONNECT;
	return GF_OK;

#else	
	struct hostent *Host;
	
	/*setup the address*/
	sock->dest_addr.sin_family = AF_INET;
	sock->dest_addr.sin_port = htons(PortNumber);
	/*get the server IP*/
	sock->dest_addr.sin_addr.s_addr = inet_addr(PeerName);
	if (sock->dest_addr.sin_addr.s_addr==INADDR_NONE) {
		Host = gethostbyname(PeerName);
		if (Host == NULL) {
			switch (LASTSOCKERROR) {
			case ENETDOWN: return GF_IP_NETWORK_FAILURE;
			case ENOHOST: return GF_IP_ADDRESS_NOT_FOUND;
			default: return GF_IP_NETWORK_FAILURE;
			}
		}
		memcpy((char *) &sock->dest_addr.sin_addr, Host->h_addr_list[0], sizeof(u32));
	}

	if (sock->type == GF_SOCK_TYPE_TCP) {
		ret = connect(sock->socket, (struct sockaddr *) &sock->dest_addr, sizeof(struct sockaddr));
		if (ret == SOCKET_ERROR) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
			case EISCONN:
				return GF_OK;
			default:
				return GF_IP_CONNECTION_FAILURE;
			}
		}
	}
#endif
	sock->status = SK_STATUS_CONNECT;
	return GF_OK;
}

//binds the given socket to the specified port. If ReUse is true
//this will enable reuse of ports on a single machine
GF_Err gf_sk_bind(GF_Socket *sock, u16 PortNumber, Bool reUse)
{
#ifdef GPAC_IPV6
	struct addrinfo *res, *aip;
#else
	size_t addrlen;
	struct sockaddr_in LocalAdd;
	struct hostent *Host;
	char buf[GF_MAX_IP_NAME_LEN];
#endif
	s32 ret;
	s32 optval;

	if (!sock || (sock->status != SK_STATUS_CREATE)) return GF_BAD_PARAM;

	if (reUse) {
		//retry with ReUsability of socket names
		optval = 1;
		setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));
	}
	
	/*IPV6 bind*/
#ifdef GPAC_IPV6
	res = gf_sk_get_ipv6_addr(NULL, PortNumber, AF_UNSPEC, AI_PASSIVE, sock->type);
	if (!res) return GF_IP_CONNECTION_FAILURE;

	/*close socket*/
	closesocket(sock->socket);
	sock->socket = (SOCKET)NULL;

	/*for all interfaces*/
	for (aip=res; aip!=NULL; aip=aip->ai_next) {
		if (sock->type != (u32) aip->ai_socktype) continue;
		sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
		if (sock->socket == INVALID_SOCKET) {
			sock->socket = (SOCKET)NULL;
			continue;
		}
		/*TODO: copy over other properties (recption buffer size & co)*/
		gf_sk_set_blocking(sock, !sock->blocking);

		ret = bind(sock->socket, aip->ai_addr, aip->ai_addrlen);
		if (ret == SOCKET_ERROR) {
			closesocket(sock->socket);
			sock->socket = (SOCKET)NULL;
			continue;
		}
		sock->is_ipv6 = (aip->ai_family==PF_INET6) ? 1 : 0;
		break;
	}
	freeaddrinfo(res);
	if (!sock->socket) return GF_IP_CONNECTION_FAILURE;
#else
	memset((void *) &LocalAdd, 0, sizeof(LocalAdd));
	ret = gethostname(buf, GF_MAX_IP_NAME_LEN);
	if (ret == SOCKET_ERROR) return GF_IP_ADDRESS_NOT_FOUND;
	/*get the IP address*/
	Host = gethostbyname(buf);
	if (Host == NULL) return GF_IP_ADDRESS_NOT_FOUND;
	/*setup the address*/
	memcpy((char *) &LocalAdd.sin_addr, Host->h_addr_list[0], sizeof(LocalAdd.sin_addr));
	LocalAdd.sin_family = AF_INET;
	LocalAdd.sin_addr.s_addr = INADDR_ANY;
	LocalAdd.sin_port = htons(PortNumber);
	addrlen = sizeof(struct sockaddr_in);
	/*bind the socket*/
	ret = bind(sock->socket, (struct sockaddr *) &LocalAdd, addrlen);
	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
#endif
	sock->status = SK_STATUS_BIND;
	return GF_OK;
}

//send length bytes of a buffer
GF_Err gf_sk_send(GF_Socket *sock, unsigned char *buffer, u32 length)
{
	GF_Err e;
	u32 Count, Res, ready;
	struct timeval timeout;
	fd_set Group;

	e = GF_OK;

	//the socket must be bound or connected
	if ((sock->status != SK_STATUS_CONNECT) && (sock->status!=SK_STATUS_BIND)) return GF_BAD_PARAM;

	//can we write?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = 0;
	timeout.tv_usec = SOCK_MICROSEC_WAIT;

	ready = select(sock->socket+1, NULL, &Group, NULL, &timeout);
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

	//direct writing
	Count = 0;
	while (Count < length) {
		if (sock->type == GF_SOCK_TYPE_TCP) {
			Res = send(sock->socket, &buffer[Count], length - Count, 0);
		} else {
			Res = sendto(sock->socket, &buffer[Count], length - Count, 0, (struct sockaddr *) &sock->dest_addr, sock->dest_addr_len);
		}
		if (Res == SOCKET_ERROR) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
			case ENOTCONN:
			case ECONNRESET:
				return GF_IP_CONNECTION_CLOSED;
			default:
				return GF_IP_NETWORK_FAILURE;
			}
		}
		Count += Res;
	}
	return GF_OK;
}


u32 gf_sk_is_multicast_address(char *multi_IPAdd)
{
#ifdef GPAC_IPV6
	u32 val;
 	struct addrinfo *res;
	if (!multi_IPAdd) return 0;
	res = gf_sk_get_ipv6_addr(multi_IPAdd, 0, AF_UNSPEC, AI_PASSIVE, SOCK_STREAM);
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

//binds MULTICAST
GF_Err gf_sk_setup_multicast(GF_Socket *sock, char *multi_IPAdd, u16 MultiPortNumber, u32 TTL, Bool NoBind, char *local_interface_ip)
{
	s32 ret;
	u32 flag; 
	struct ip_mreq M_req;
	u32 optval;
#ifdef GPAC_IPV6
	struct sockaddr *addr;
	struct addrinfo *res, *aip;
#else
	u_long local_add_id;
#endif

	if (!sock || (sock->status != SK_STATUS_CREATE)) return GF_BAD_PARAM;

	if (TTL > 255) TTL = 255;
	
	/*check the address*/
	if (!gf_sk_is_multicast_address(multi_IPAdd)) return GF_BAD_PARAM;

#ifdef GPAC_IPV6

	res = gf_sk_get_ipv6_addr(local_interface_ip, MultiPortNumber, AF_UNSPEC, AI_PASSIVE, sock->type);
	if (!res) {
		if (local_interface_ip) {
			res = gf_sk_get_ipv6_addr(NULL, MultiPortNumber, AF_UNSPEC, AI_PASSIVE, sock->type);
			local_interface_ip = NULL;
		}
		if (!res) return GF_IP_CONNECTION_FAILURE;
	}

	/*close socket*/
	closesocket(sock->socket);
	sock->socket = (SOCKET)NULL;

	/*for all interfaces*/
	for (aip=res; aip!=NULL; aip=aip->ai_next) {
		if (sock->type != (u32) aip->ai_socktype) continue;
		sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
		if (sock->socket == INVALID_SOCKET) {
			sock->socket = (SOCKET)NULL;
			continue;
		}
		/*enable address reuse*/
		optval = SO_REUSEADDR;
		setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));

		/*TODO: copy over other properties (recption buffer size & co)*/
		gf_sk_set_blocking(sock, !sock->blocking);

		if (!NoBind) {
			ret = bind(sock->socket, aip->ai_addr, aip->ai_addrlen);
			if (ret == SOCKET_ERROR) {
				closesocket(sock->socket);
				sock->socket = (SOCKET)NULL;
				continue;
			}
		}
		sock->is_ipv6 = (aip->ai_family==AF_INET6) ? 1 : 0;
		break;
	}
	freeaddrinfo(res);
	if (!sock->socket) return GF_IP_CONNECTION_FAILURE;

	sock->status = SK_STATUS_BIND;

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
        ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *) &M_reqV6, sizeof(M_reqV6));
		if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		/*set TTL*/
		ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &TTL, sizeof(TTL));
		if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		/*Disable loopback*/
		flag = 1;
		ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &flag, sizeof(flag));
		if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
	} 
#else

	/*enable address reuse*/
	optval = SO_REUSEADDR;
	setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));

	if (local_interface_ip) local_add_id = inet_addr(local_interface_ip);
	else local_add_id = htonl(INADDR_ANY);

	if (!NoBind) {
		SOCKADDR_IN local_address;

		local_address.sin_family = AF_INET;
		local_address.sin_addr.s_addr = local_add_id;
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
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_IF, (char *) &local_add_id, sizeof(local_add_id));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		}
	}
	sock->status = SK_STATUS_BIND;

	/*now join the multicast*/
	M_req.imr_multiaddr.s_addr = inet_addr(multi_IPAdd);
	M_req.imr_interface.s_addr = local_add_id;

	ret = setsockopt(sock->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &M_req, sizeof(M_req));
	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
	/*set the Time To Live*/
	ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&TTL, sizeof(TTL));
	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
	/*Disable loopback*/
	flag = 1;
	ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &flag, sizeof(flag));
	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;

	sock->dest_addr.sin_family = AF_INET;
	sock->dest_addr.sin_addr.s_addr = M_req.imr_multiaddr.s_addr;
	sock->dest_addr.sin_port = htons( MultiPortNumber);
#endif
	return GF_OK;
}




//fetch nb bytes on a socket and fill the buffer from startFrom
//length is the allocated size of the receiving buffer
//BytesRead is the number of bytes read from the network
GF_Err gf_sk_receive(GF_Socket *sock, unsigned char *buffer, u32 length, u32 startFrom, u32 *BytesRead)
{
	GF_Err e;
	u32 res, ready;
	struct timeval timeout;
	fd_set Group;

	e = GF_OK;

	*BytesRead = 0;
	if (startFrom >= length) return 0;


	//can we read?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = 0;
	timeout.tv_usec = SOCK_MICROSEC_WAIT;

	res = 0;
	ready = select(sock->socket+1, &Group, NULL, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
//			fprintf(stdout, "error fetching %d\n", LASTSOCKERROR);
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!FD_ISSET(sock->socket, &Group)) {
		return GF_IP_NETWORK_EMPTY;
	}

	res = recv(sock->socket, buffer + startFrom, length - startFrom, 0);
	if (res == SOCKET_ERROR) {
		res = LASTSOCKERROR;
		switch (res) {
		case EMSGSIZE:
			return GF_OUT_OF_MEM;
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		case ENOTCONN:
		case ECONNRESET:
		case ECONNABORTED:
			return GF_IP_CONNECTION_CLOSED;
		default:
//			fprintf(stdout, "error fetching %d\n", res);
			return GF_IP_NETWORK_FAILURE;
		}
	}
	*BytesRead = res;
	return GF_OK;
}


GF_Err gf_sk_listen(GF_Socket *sock, u32 MaxConnection)
{
	s32 i;
	if (sock->status != SK_STATUS_BIND) return GF_BAD_PARAM;
	if (MaxConnection >= SOMAXCONN) MaxConnection = SOMAXCONN;
	i = listen(sock->socket, MaxConnection);
	if (i == SOCKET_ERROR) return GF_IP_NETWORK_FAILURE;
	sock->status = SK_STATUS_LISTEN;
	return GF_OK;
}

GF_Err gf_sk_accept(GF_Socket *sock, GF_Socket **newConnection)
{
	u32 client_address_size, ready, res;
	SOCKET sk;
	struct timeval timeout;
	fd_set Group;

	*newConnection = NULL;
	if (sock->status != SK_STATUS_LISTEN) return GF_BAD_PARAM;

	//can we read?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = 0;
	timeout.tv_usec = SOCK_MICROSEC_WAIT;

	res = 0;
	ready = select(sock->socket, &Group, NULL, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!ready || !FD_ISSET(sock->socket, &Group)) return GF_IP_NETWORK_EMPTY;
#ifdef GPAC_IPV6
	client_address_size = sizeof(struct sockaddr_in6);
#else
	client_address_size = sizeof(struct sockaddr_in);
#endif
	sk = accept(sock->socket, (struct sockaddr *) &sock->dest_addr, &client_address_size);

	//we either have an error or we have no connections
	if (sk == INVALID_SOCKET) {
		if (sock->blocking) return GF_IP_NETWORK_FAILURE;
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}		
	}

	(*newConnection) = malloc(sizeof(GF_Socket));
	(*newConnection)->socket = sk;
	(*newConnection)->type = sock->type;
	(*newConnection)->blocking = sock->blocking;
	(*newConnection)->status = SK_STATUS_CONNECT;
#ifdef GPAC_IPV6
	memcpy( &(*newConnection)->dest_addr, &sock->dest_addr, client_address_size);
	memset(&sock->dest_addr, 0, sizeof(struct sockaddr_in6));
#else
	memcpy( &(*newConnection)->dest_addr, &sock->dest_addr, client_address_size);
	memset(&sock->dest_addr, 0, sizeof(struct sockaddr_in));
#endif
	(*newConnection)->dest_addr_len = client_address_size;
	return GF_OK;
}

GF_Err gf_sk_get_local_info(GF_Socket *sock, u16 *Port, u32 *Familly)
{
#ifdef GPAC_IPV6
	struct sockaddr_in6 the_add;
#else
	struct sockaddr_in the_add;
#endif
	u32 size, fam;

	*Port = 0;
	*Familly = 0;

	if (!sock || sock->status != SK_STATUS_CONNECT) return GF_BAD_PARAM;

#ifdef GPAC_IPV6
	size = sizeof(struct sockaddr_in6);
	if (getsockname(sock->socket, (struct sockaddr *) &the_add, &size) == SOCKET_ERROR) return GF_IP_NETWORK_FAILURE;
	*Port = (u32) ntohs(the_add.sin6_port);
#else
	size = sizeof(struct sockaddr_in);
	if (getsockname(sock->socket, (struct sockaddr *) &the_add, &size) == SOCKET_ERROR) return GF_IP_NETWORK_FAILURE;
	*Port = (u32) ntohs(the_add.sin_port);
#endif

	size = 4;
	if (getsockopt(sock->socket, SOL_SOCKET, SO_TYPE, (char *) &fam, &size) == SOCKET_ERROR)
		return GF_IP_NETWORK_FAILURE;

	*Familly = fam;
	return GF_OK;
}

//we have to do this for the server sockets as we use only one thread 
GF_Err gf_sk_server_mode(GF_Socket *sock, Bool serverOn)
{
	u32 one;

	if (!sock || (sock->type != GF_SOCK_TYPE_TCP) || (sock->status != SK_STATUS_CONNECT) )
		return GF_BAD_PARAM;

	one = serverOn ? 1 : 0;
	setsockopt(sock->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(u32));
	setsockopt(sock->socket, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(u32));
	return GF_OK;
}

GF_Err gf_sk_get_remote_address(GF_Socket *sock, char *buf)
{
#ifdef GPAC_IPV6
	char clienthost[NI_MAXHOST];
 	struct sockaddr_in6 * addrptr = (struct sockaddr_in6 *)(&sock->dest_addr_len);
	if (!sock || sock->status != SK_STATUS_CONNECT) return GF_BAD_PARAM;
	if (getnameinfo((struct sockaddr *)addrptr, sock->dest_addr_len, clienthost, sizeof(clienthost), NULL, 0, NI_NUMERICHOST)) 
		return GF_IP_ADDRESS_NOT_FOUND;
	strcpy(buf, clienthost);
#else
	if (!sock || sock->status != SK_STATUS_CONNECT) return GF_BAD_PARAM;
	strcpy(buf, inet_ntoa(sock->dest_addr.sin_addr));
#endif
	return GF_OK;	
}


GF_Err gf_sk_set_remote_address(GF_Socket *sock, char *address)
{
#ifdef GPAC_IPV6
	if (!sock || !address) return GF_BAD_PARAM;
	if (!gf_sk_ipv6_set_remote_address(sock, address, 0)) return GF_IP_ADDRESS_NOT_FOUND;
	return GF_OK;
#else
	struct hostent *Host;
	if (!sock || !address) return GF_BAD_PARAM;
	sock->dest_addr.sin_family = AF_INET;
	sock->dest_addr.sin_addr.S_un.S_addr = inet_addr(address);
	if (sock->dest_addr.sin_addr.S_un.S_addr == INADDR_NONE) {
		Host = gethostbyname(address);
		if (Host == NULL) return GF_IP_ADDRESS_NOT_FOUND;
		memcpy((char *) &sock->dest_addr.sin_addr, Host->h_addr_list[0], sizeof(u32));
	}
#endif
	return GF_OK;
}

GF_Err gf_sk_set_remote_port(GF_Socket *sock, u16 RemotePort)
{
#ifdef GPAC_IPV6
	struct sockaddr *addr = (struct sockaddr *)&sock->dest_addr;
	if (addr->sa_family == AF_INET) ((struct sockaddr_in *)addr)->sin_port =  htons(RemotePort);
	else if (addr->sa_family == AF_INET6) ((struct sockaddr_in6 *)addr)->sin6_port =  htons(RemotePort);
#else
	if (!sock || !RemotePort) return GF_BAD_PARAM;
	sock->dest_addr.sin_port = htons(RemotePort);
#endif
	return GF_OK;
}


//send length bytes of a buffer
GF_Err gf_sk_send_to(GF_Socket *sock, unsigned char *buffer, u32 length, unsigned char *remoteHost, u16 remotePort)
{
	u32 Count, Res, ready, remote_add_len;
#ifdef GPAC_IPV6
	struct sockaddr_storage remote_add;
#else
	struct sockaddr_in remote_add;
	struct hostent *Host;
#endif
	struct timeval timeout;
	fd_set Group;

	//the socket must be bound or connected
	if ((sock->status != SK_STATUS_BIND) && (sock->status != SK_STATUS_CONNECT)) return GF_BAD_PARAM;
	if (remoteHost && !remotePort) return GF_BAD_PARAM;

	//can we write?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = 0;
	timeout.tv_usec = SOCK_MICROSEC_WAIT;

	ready = select(sock->socket+1, NULL, &Group, NULL, &timeout);
	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		default:
			return GF_IP_NETWORK_FAILURE;
		}
	}
	//to check: writeability is guaranteed for not-connected sockets
	if (sock->status == SK_STATUS_CONNECT) {
		if (!ready || !FD_ISSET(sock->socket, &Group)) return GF_IP_NETWORK_EMPTY;
	}

	/*setup the address*/
#ifdef GPAC_IPV6
	remote_add.ss_family = AF_INET6;
	//if a remote host is specified, use it. Otherwise use the default host
	if (remoteHost) {
		//setup the address
		struct addrinfo *res = gf_sk_get_ipv6_addr(remoteHost, remotePort, AF_UNSPEC, 0, sock->type);
		if (!res) return GF_IP_ADDRESS_NOT_FOUND;
		memcpy(&remote_add, res->ai_addr, res->ai_addrlen);
		remote_add_len = res->ai_addrlen;
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
	Count = 0;
	while (Count < length) {
		Res = sendto(sock->socket, &buffer[Count], length - Count, 0, (struct sockaddr *) &remote_add, remote_add_len); 		
		if (Res == SOCKET_ERROR) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
			default:
				return GF_IP_NETWORK_FAILURE;
			}
		}
		Count += Res;
	}
	return GF_OK;
}




GF_Err gf_sk_receive_wait(GF_Socket *sock, unsigned char *buffer, u32 length, u32 startFrom, u32 *BytesRead, u32 Second )
{
	GF_Err e;
	u32 res, ready;
	struct timeval timeout;
	fd_set Group;

	e = GF_OK;

	*BytesRead = 0;
	if (startFrom >= length) return 0;


	//can we read?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = Second;
	timeout.tv_usec = SOCK_MICROSEC_WAIT;

	res = 0;
	ready = select(sock->socket+1, &Group, NULL, NULL, &timeout);
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

	res = recv(sock->socket, buffer + startFrom, length - startFrom, 0);
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
GF_Err gf_sk_send_wait(GF_Socket *sock, unsigned char *buffer, u32 length, u32 Second )
{

	GF_Err e;
	u32 Count, Res, ready;
	struct timeval timeout;
	fd_set Group;

	e = GF_OK;

	//the socket must be bound or connected
	if (sock->status != SK_STATUS_CONNECT) return GF_BAD_PARAM;

	//can we write?
	FD_ZERO(&Group);
	FD_SET(sock->socket, &Group);
	timeout.tv_sec = Second;
	timeout.tv_usec = SOCK_MICROSEC_WAIT;

	ready = select(sock->socket+1, NULL, &Group, NULL, &timeout);
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

	//direct writing
	Count = 0;
	while (Count < length) {
		Res = send(sock->socket, &buffer[Count], length - Count, 0);
		if (Res == SOCKET_ERROR) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_SOCK_WOULD_BLOCK;
			case ECONNRESET:
				return GF_IP_CONNECTION_CLOSED;
			default:
				return GF_IP_NETWORK_FAILURE;
			}
		}
		Count += Res;
	}
	return GF_OK;
}








//Socket Group for select(). The group is a collection of sockets ready for reading / writing
typedef struct __tag_sock_group
{
	//the max time value before a select returns
	struct timeval timeout;
	fd_set ReadGroup;
	fd_set WriteGroup;
} GF_SocketGroup;


#define GF_SOCK_GROUP_READ 0
#define GF_SOCK_GROUP_WRITE 1

GF_SocketGroup *NewSockGroup()
{	
	GF_SocketGroup *tmp = malloc(sizeof(GF_SocketGroup));
	if (!tmp) return NULL;
	FD_ZERO(&tmp->ReadGroup);
	FD_ZERO(&tmp->WriteGroup);
	return tmp;
}

void SKG_Delete(GF_SocketGroup *group)
{
	free(group);
}

void SKG_SetWatchTime(GF_SocketGroup *group, u32 DelayInS, u32 DelayInMicroS)
{
	group->timeout.tv_sec = DelayInS;
	group->timeout.tv_usec = DelayInMicroS;
}

void SKG_AddSocket(GF_SocketGroup *group, GF_Socket *sock, u32 GroupType)
{

	switch (GroupType) {
	case GF_SOCK_GROUP_READ:
		FD_SET(sock->socket, &group->ReadGroup);
		return;
	case GF_SOCK_GROUP_WRITE:
		FD_SET(sock->socket, &group->WriteGroup);
		return;
	default:
		return;
	}
}

void SKG_RemoveSocket(GF_SocketGroup *group, GF_Socket *sock, u32 GroupType)
{
	switch (GroupType) {
	case GF_SOCK_GROUP_READ:
		FD_CLR(sock->socket, &group->ReadGroup);
		return;
	case GF_SOCK_GROUP_WRITE:
		FD_CLR(sock->socket, &group->WriteGroup);
		return;
	default:
		return;
	}
}


Bool SKG_IsSocketIN(GF_SocketGroup *group, GF_Socket *sock, u32 GroupType)
{

	switch (GroupType) {
	case GF_SOCK_GROUP_READ:
		if (FD_ISSET(sock->socket, &group->ReadGroup)) return 1;
		return 0;
	case GF_SOCK_GROUP_WRITE:
		if (FD_ISSET(sock->socket, &group->WriteGroup)) return 1;
		return 0;
	default:
		return 0;
	}
}

u32 SKG_Select(GF_SocketGroup *group)
{
	u32 ready, rien = 0;
	ready = select(rien, &group->ReadGroup, &group->WriteGroup, NULL, &group->timeout);
	if (ready == SOCKET_ERROR) return 0;
	return ready;
}


