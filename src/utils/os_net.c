/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#ifndef GPAC_DISABLE_NETWORK

#include <gpac/utf.h>

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
#pragma comment(lib, "IPHLPAPI.lib")
#define GPAC_WIN_HAS_ADAPTER_INFO
#endif

#if !defined(POLLIN)
#ifdef GPAC_HAS_POLL
#undef GPAC_HAS_POLL
#endif
#endif

#ifdef GPAC_HAS_POLL
#define GF_POLLFD	WSAPOLLFD
#define poll WSAPoll
#endif

#endif

#include <windows.h>

#if defined(IPV6_MULTICAST_IF)
#define GPAC_HAS_IPV6 1
#else
#undef GPAC_HAS_IPV6
#endif

 /*
 #if !defined(__GNUC__)
#if defined(GPAC_HAS_IPV6)
#pragma message("Using WinSock IPV6")
#else
#pragma message("Using WinSock IPV4")
#endif

#endif
*/

#include <errno.h>

#ifndef EISCONN
/*common win32 redefs*/
#undef EAGAIN
#define EAGAIN				WSAEWOULDBLOCK
#define EISCONN				WSAEISCONN
#define ENOTCONN			WSAENOTCONN
#define ECONNRESET			WSAECONNRESET
#define EINPROGRESS			WSAEINPROGRESS
#define EALREADY			WSAEALREADY
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

#ifdef GPAC_HAS_POLL
#include <poll.h>

#define GF_POLLFD	struct pollfd

#endif

#endif /*WIN32||_WIN32_WCE*/

#ifdef GPAC_BUILD_FOR_WINXP

/* Added by M. Lackner to ensure Windows XP & XP x64 compatibility */
/* Adding PlibC's inet_ntop() function in a very slightly modified
and renamed variant for IPv6 address conversion support on Win XP's,
which is called by gf_sk_get_remote_address() further below.
The renamed function is called inet_ntop_xp().
Original source code taken from here:
https://sourceforge.net/projects/plibc/

This inet_ntop() implementation is (C) 1996-2001 Internet Software
Consortium and is licensed under the ISC license.
License text: https://opensource.org/licenses/ISC */
#define NS_INT16SZ   2
#define NS_IN6ADDRSZ  16

/*
* WARNING: Don't even consider trying to compile this on a system where
* sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
*/

static const char *inet_ntop4(const unsigned char *src, char *dst, size_t size);

#ifdef AF_INET6
static const char *inet_ntop6(const unsigned char *src, char *dst, size_t size);
#endif

/* char *
* isc_net_ntop(af, src, dst, size)
*  convert a network format address to presentation format.
* return:
*  pointer to presentation format address (`dst'), or NULL (see errno).
* author:
*  Paul Vixie, 1996.
*/
const char * inet_ntop_xp(int af, const void *src, char *dst, size_t size)
{
	switch (af) {
	case AF_INET:
		return (inet_ntop4(src, dst, size));
#ifdef AF_INET6
	case AF_INET6:
		return (inet_ntop6(src, dst, size));
#endif
	default:
		errno = EAFNOSUPPORT;
		return (NULL);
	}
	/* NOTREACHED */
}

/* const char *
* inet_ntop4(src, dst, size)
*  format an IPv4 address
* return:
*  `dst' (as a const)
* notes:
*  (1) uses no statics
*  (2) takes a unsigned char* not an in_addr as input
* author:
*  Paul Vixie, 1996.
*/
static const char * inet_ntop4(const unsigned char *src, char *dst, size_t size)
{
	static const char *fmt = "%u.%u.%u.%u";
	char tmp[sizeof "255.255.255.255"];
	size_t len;

	len = snprintf(tmp, sizeof tmp, fmt, src[0], src[1], src[2], src[3]);
	if (len >= size) {
		errno = ENOSPC;
		return (NULL);
	}
	memcpy(dst, tmp, len + 1);

	return (dst);
}

/* const char *
* isc_inet_ntop6(src, dst, size)
*  convert IPv6 binary address into presentation (printable) format
* author:
*  Paul Vixie, 1996.
*/
#ifdef AF_INET6
static const char * inet_ntop6(const unsigned char *src, char *dst, size_t size)
{
	/*
	* Note that int32_t and int16_t need only be "at least" large enough
	* to contain a value of the specified size.  On some systems, like
	* Crays, there is no such thing as an integer variable with 16 bits.
	* Keep this in mind if you think this function should have been coded
	* to use pointer overlays.  All the world's not a VAX.
	*/
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct { int base, len; } best, cur;
	unsigned int words[NS_IN6ADDRSZ / NS_INT16SZ];
	int i, inc;

	/*
	* Preprocess:
	*  Copy the input (bytewise) array into a wordwise array.
	*  Find the longest run of 0x00's in src[] for :: shorthanding.
	*/
	memset(words, '\0', sizeof words);
	for (i = 0; i < NS_IN6ADDRSZ; i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	best.len = 0;
	cur.base = -1;
	cur.len = 0;
	for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		}
		else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	* Format the result.
	*/
	tp = tmp;
	for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base && i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			if (!inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		inc = snprintf(tp, 5, "%x", words[i]);
		tp += inc;
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == (NS_IN6ADDRSZ / NS_INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/*
	* Check for overflow, copy, and we're done.
	*/
	if ((size_t)(tp - tmp) > size) {
		errno = ENOSPC;
		return (NULL);
	}
	memcpy(dst, tmp, tp - tmp);
	return (dst);
}
#endif /* AF_INET6 */
/* End of inet_ntop() reimplementation */


#endif //winxp

#ifdef GPAC_BUILD_FOR_WINXP
#define my_inet_ntop inet_ntop_xp
#else
#define my_inet_ntop inet_ntop
#endif


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

#ifdef GPAC_HAS_SOCK_UN
#include <sys/un.h>
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
	GF_SOCK_IS_UN = 1<<15,
	GF_SOCK_HAS_CONNECT = 1<<16,
};

#ifndef GPAC_DISABLE_NETCAP

typedef struct __netcap_filter GF_NetcapFilter;
static GF_NetcapFilter *gf_net_filter_get(const char *id);

typedef struct
{
	//local host info
	u32 host_addr_v4;
	bin128 host_addr_v6;
	u32 host_port;
	//remote peer info
	u32 peer_addr_v4;
	bin128 peer_addr_v6;
	u32 peer_port;

	u32 pck_idx_r, pck_idx_w, next_pck_range, next_rand;
	u32 patch_offset, patch_val;
	GF_NetcapFilter *nf;
} NetCapInfo;
#endif


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
#ifdef GPAC_HAS_POLL
	u32 poll_idx;
#endif

#ifndef GPAC_DISABLE_NETCAP
	NetCapInfo *cap_info;
#endif
};



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

#if defined(GPAC_WIN_HAS_ADAPTER_INFO)
#include <iphlpapi.h>
#endif

#if defined(GPAC_HAS_IFADDRS)
#include <ifaddrs.h>
#include <net/if.h>
#endif


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
		strncpy(node, PeerName, MAX_PEER_NAME_LEN-1);
		if (node[0]=='[') {
			node[strlen(node)-1] = 0;
			memmove(node, &node[1], MAX_PEER_NAME_LEN-1);
		}
		node[MAX_PEER_NAME_LEN - 1] = 0;
		dest = (char *) node;
	}
	if (getaddrinfo((const char *)dest, (const char *)service, &hints, &res) != 0) return NULL;
	return res;
}

static struct addrinfo *gf_sk_get_ifce_ipv6_addr(const char *ifce_name_or_ip, u16 PortNumber, int family, int flags, int sock_type)
{
	char szIP[100];
	Bool force_ipv6 = GF_FALSE;
	if (!ifce_name_or_ip || !ifce_name_or_ip[0] || gf_net_is_ipv6(ifce_name_or_ip) || strchr(ifce_name_or_ip, '.'))
		return gf_sk_get_ipv6_addr(ifce_name_or_ip, PortNumber, family, flags, sock_type);
	if (ifce_name_or_ip[0]=='+') {
		force_ipv6 = GF_TRUE;
		ifce_name_or_ip+=1;
		if (!ifce_name_or_ip[0]) {
			return gf_sk_get_ipv6_addr(ifce_name_or_ip, PortNumber, AF_INET6, flags, sock_type);
		}
	}
	if (family==AF_INET6) force_ipv6 = GF_TRUE;

	szIP[0] = 0;

#if defined(GPAC_WIN_HAS_ADAPTER_INFO)
	IP_ADAPTER_ADDRESSES *AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES));
	ULONG ulOutBufLen = sizeof(AdapterInfo);
	if (AdapterInfo && (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)) {
		free(AdapterInfo);
		AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES)*ulOutBufLen);
	}
	if (AdapterInfo)
		GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterInfo, &ulOutBufLen);
	IP_ADAPTER_ADDRESSES *ifa = AdapterInfo;
	while (ifa) {
		Bool match = GF_FALSE;
		if (!strcmp(ifa->AdapterName, ifce_name_or_ip)) match = GF_TRUE;
		char *name = gf_wcs_to_utf8(ifa->FriendlyName);
		if (name) {
			if (!strcmp(name, ifce_name_or_ip)) match = GF_TRUE;
			gf_free(name);
		}
		if (!match) {
			ifa = ifa->Next;
			szIP[0] = 0;
			continue;
		}
		IP_ADAPTER_UNICAST_ADDRESS_LH *ipadd = ifa->FirstUnicastAddress;
		while (ipadd) {
			if (!ipadd->Address.lpSockaddr) {
				ipadd = ipadd->Next;
				continue;
			}
			if (ipadd->Address.lpSockaddr->sa_family==AF_INET) {
				my_inet_ntop(ipadd->Address.lpSockaddr->sa_family, ipadd->Address.lpSockaddr, szIP, 100);
				free(AdapterInfo);
				return gf_sk_get_ipv6_addr(szIP, PortNumber, family, flags, sock_type);
			}
			if (ipadd->Address.lpSockaddr->sa_family == AF_INET6) {
				my_inet_ntop(ipadd->Address.lpSockaddr->sa_family, ipadd->Address.lpSockaddr, szIP, 100);
				if (force_ipv6) {
					free(AdapterInfo);
					return gf_sk_get_ipv6_addr(szIP, PortNumber, family, flags, sock_type);
				}
			}
			ipadd = ipadd->Next;
		}
		ifa = ifa->Next;
	}
	if (AdapterInfo)
		free(AdapterInfo);
#endif

#if defined(GPAC_HAS_IFADDRS)
	struct ifaddrs *ifap = NULL, *ifa;
	getifaddrs(&ifap);
	ifa = ifap;
	while (ifa) {
		if (!ifa->ifa_addr || strcmp(ifa->ifa_name, ifce_name_or_ip)) {
			ifa = ifa->ifa_next;
			continue;
		}
		if (ifa->ifa_addr->sa_family==AF_INET) {
			memset(szIP, 0, 100);
			inet_ntop(AF_INET, &( ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr), szIP, 100);
			freeifaddrs(ifap);
			return gf_sk_get_ipv6_addr(szIP, PortNumber, family, flags, sock_type);
		}
		if (ifa->ifa_addr->sa_family==AF_INET6) {
			memset(szIP, 0, 100);
			inet_ntop(AF_INET6, &( ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr), szIP, 100);
			if (force_ipv6) {
				freeifaddrs(ifap);
				return gf_sk_get_ipv6_addr(szIP, PortNumber, family, flags, sock_type);
			}
		}
		ifa = ifa->ifa_next;
	}
	freeifaddrs(ifap);
#endif

	if (szIP[0]) {
		return gf_sk_get_ipv6_addr(szIP, PortNumber, family, flags, sock_type);
	}
	return NULL;
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
		if (sock->flags & GF_SOCK_IS_MULTICAST)
			return GF_BAD_PARAM;
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

#ifndef GPAC_DISABLE_NETCAP

#include <gpac/bitstream.h>
#include <gpac/list.h>

enum
{
	NETCAP_IS_MCAST = 1,
	NETCAP_IS_TCP = 1<<1,
	NETCAP_HAS_PEER = 1<<2,
	NETCAP_IS_IPV6 = 1<<3,
	NETCAP_HAS_EXT = 1<<6,
	NETCAP_DROP = 1<<7,
};

//capture mode only, target bitstream
u16 first_tcp_port=30000;

#define GF_GPC_MAGIC	GF_4CC('G','P','C', 0)

//playback vars


enum {
	NETCAP_NONE = 0,
	//read/write
	NETCAP_GPAC = 1,
	//read only
	NETCAP_PCAP = 2,
	NETCAP_PCAPNG = 3,
};

typedef struct
{
	u32 send_recv;
	u32 pck_start, pck_end;
	s32 rand_every;
	u32 nb_pck;
	u32 port;
	s32 patch_offset, patch_val;
} NetFilterRule;

static GF_List *netcap_filters=NULL;
#ifdef GPAC_HAS_FD
GF_BitStream *gf_bs_from_fd(int fd, u32 mode);
#endif

struct __netcap_filter
{
	char *id;
	char *src;
	char *dst;
	s32 loops;
	Bool rt;
	GF_List *rules;
	u32 nb_rules;


	//target bitstream for read/write
	GF_BitStream *cap_bs;
	//playback mode only, set of created sockets, used to discard packets on non-bound sockets
	GF_List *read_socks;
	//playback mode only, set to socket corresponding to pending packet
	GF_Socket *read_sock_selected;
	//playback/capture, FILE pointer
	FILE *file;
#ifdef GPAC_HAS_FD
	//playback/capture, fd pointer
	int fd;
#endif

	//playback only, clock init value for RT regulation
	u64 init_time;
	Bool is_eos;


	//loaded packet size in bytes - this is the payload of the UDP/TCP packet - if 0, packet fetch failed
	s32 pck_len;
	//gpac format only, flags of loaded packet
	u8 pck_flags;
	//time of loaded packet, relative to sys_clock_high_res
	u64 pck_time;
	//destination port of loaded packet - if 0, new packet needs to be loaded
	u16 dst_port;
	//ipv4 dest address or mcast address of loaded packet
	u32 dst_v4;
	//ipv6 dest address or mcast address of loaded packet
	bin128 dst_v6;
	//source port of loaded packet
	u16 src_port;
	//ipv4 source address of loaded packet
	u32 src_v4;
	//ipv6 source address of loaded packet
	bin128 src_v6;

	//source file type
	u32 cap_mode;
	//for pcap files, indicate if little endian
	Bool pcap_le;
	//for pcap files, indicate nano seconds are used
	Bool pcap_nano;
	//number of interfaces currently defined - for pcap, always, 1
	u32 pcap_num_interfaces;
	//all defined interfaces
	u32 link_types[20];
	//trailing bytes in pcapng packet record
	u32 pcapng_trail;
	//trailing bytes after packet in pcap (NOT in pcapng)
	u32 pcap_trail;

	u32 pck_idx_r;
	u32 pck_idx_w;
	u32 next_pck_range;
	u32 next_rand;
};

static GF_NetcapFilter *gf_net_filter_get(const char *id)
{
	u32 i, count;
	count = gf_list_count(netcap_filters);
	for (i=0; i<count; i++) {
		GF_NetcapFilter *f = gf_list_get(netcap_filters, i);
		if (f->id && id && !strcmp(f->id, id))
			return f;

		if (!f->id && !id)
			return f;
	}
	return NULL;
}
//end playback vars



void gf_net_close_capture()
{
	while (gf_list_count(netcap_filters)) {
		GF_NetcapFilter *nf = gf_list_pop_back(netcap_filters);

		if (nf->cap_bs) gf_bs_del(nf->cap_bs);

#ifdef GPAC_HAS_FD
		if (nf->fd>=0) close(nf->fd);
#endif
		if (nf->file) gf_fclose(nf->file);
		if (nf->read_socks) gf_list_del(nf->read_socks);

		if (nf->rules) {
			while (1) {
				NetFilterRule *r = gf_list_pop_back(nf->rules);
				if (!r) break;
				gf_free(r);
			}
			gf_list_del(nf->rules);
		}
		if (nf->id) gf_free(nf->id);
		if (nf->src) gf_free(nf->src);
		if (nf->dst) gf_free(nf->dst);
		gf_free(nf);
	}
	gf_list_del(netcap_filters);
	netcap_filters = NULL;
}

static void gf_netcap_record(GF_NetcapFilter *nf)
{
#ifdef GPAC_HAS_FD
	if (!gf_opts_get_bool("core", "no-fd")) {
		//make sure output dir exists
		gf_fopen(nf->dst, "mkdir");
		nf->fd = open(nf->dst, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
		if (nf->fd>=0) nf->cap_bs = gf_bs_from_fd(nf->fd, GF_BITSTREAM_WRITE);
	} else
#endif
	{
		nf->file = gf_fopen_ex(nf->dst, NULL, "w+b", GF_FALSE);
		if (nf->file) nf->cap_bs = gf_bs_from_file(nf->file, GF_BITSTREAM_WRITE);
	}
	if (!nf->cap_bs) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Failed to setup net capture\n"));
		exit(1);
	}
	nf->cap_mode = NETCAP_GPAC;
	gf_bs_write_u32(nf->cap_bs, GF_GPC_MAGIC);
	nf->init_time = gf_sys_clock_high_res();
	u32 sec, frac;
	gf_net_get_ntp(&sec, &frac);
	//NTP start time of capture
	gf_bs_write_u32(nf->cap_bs, sec);
	gf_bs_write_u32(nf->cap_bs, frac);
	//header extension size
	gf_bs_write_u32(nf->cap_bs, 0);
}

static void gf_netcap_load_pck_gpac(GF_NetcapFilter *nf)
{
	if (gf_bs_available(nf->cap_bs)>4)
		nf->pck_len = gf_bs_read_u32(nf->cap_bs);
	else
		nf->pck_len = 0;

	//we allow size to be equal to our magic for file concat
	//this means pck size= 1196442368 (for v0) so we should be way over captured MTU
	if (nf->pck_len == GF_GPC_MAGIC) {
		/*ntp_sec = */gf_bs_read_u32(nf->cap_bs);
		/*ntp_frac = */gf_bs_read_u32(nf->cap_bs);
		u32 hdr_len = gf_bs_read_u32(nf->cap_bs);
		gf_bs_skip_bytes(nf->cap_bs, hdr_len);
		nf->pck_flags = NETCAP_DROP;
		nf->pck_len = 0;
		nf->dst_port = 1;
		return;
	}

	if (nf->pck_len<1+8+4+2) {
		nf->dst_v4 = 0;
		nf->dst_port = 0;
		nf->pck_len = 0;
		nf->is_eos = GF_TRUE;
		return;
	}
	nf->pck_len -= 1+8+2;
	//read flags
	nf->pck_flags = gf_bs_read_u8(nf->cap_bs);
	nf->pck_time = gf_bs_read_u64(nf->cap_bs);
	//extensions
	if (nf->pck_flags & NETCAP_HAS_EXT) {
		u16 xlen = gf_bs_read_u16(nf->cap_bs);
		gf_bs_skip_bytes(nf->cap_bs, xlen);
		nf->pck_len -= 2 + xlen;
	}

	//init clock on first packet
	if (!nf->init_time) {
		nf->init_time = gf_sys_clock_high_res();
		nf->init_time -= nf->pck_time;
	}
	nf->pck_time += nf->init_time;
	if (nf->pck_flags & NETCAP_IS_IPV6) {
		nf->dst_v4 = 0;
		gf_bs_read_data(nf->cap_bs, nf->dst_v6, 16);
		nf->pck_len -= 16;
		if (nf->pck_flags & NETCAP_HAS_PEER) {
			gf_bs_read_data(nf->cap_bs, nf->src_v6, 16);
			nf->pck_len -= 16;
		}
	} else {
		nf->dst_v4 = gf_bs_read_u32(nf->cap_bs);
		nf->pck_len -= 4;
		if (nf->pck_flags & NETCAP_HAS_PEER) {
			nf->src_v4 = gf_bs_read_u32(nf->cap_bs);
			nf->pck_len -= 4;
		}
	}
	nf->dst_port = gf_bs_read_u16(nf->cap_bs);
	if (nf->pck_flags & NETCAP_HAS_PEER) {
		nf->src_port = gf_bs_read_u16(nf->cap_bs);
		nf->pck_len -= 2;
	} else {
		nf->src_port = 0;
	}

	//broken packet
	if (nf->pck_len <0) {
		nf->dst_v4 = 0;
		nf->dst_port = 0;
		nf->pck_len = 0;
		nf->is_eos = GF_TRUE;
	}
}

static void pcapng_load_shb(GF_NetcapFilter *nf)
{
	u32 blen = gf_bs_read_u32(nf->cap_bs);
	u32 le_magic = gf_bs_read_u32(nf->cap_bs);
	if (le_magic == GF_4CC(0x1A, 0x2B, 0x3C, 0x4D)) {
	} else if (le_magic == GF_4CC(0x4D, 0x3C, 0x2B, 0x1A)) {
		nf->pcap_le = GF_TRUE;
		blen = ntohl(blen);
	} else {
		gf_net_close_capture();
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Corrrupted pacpng file\n"));
		exit(1);
	}
	gf_bs_skip_bytes(nf->cap_bs, blen-12);
	nf->pcap_num_interfaces = 0;
}

static void pcapng_load_idb(GF_NetcapFilter *nf)
{
	if (nf->pcap_num_interfaces>=20) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Maximum 20 concurrent interfaces supported in pacpng, skipping packets\n"));
		return;
	}
	u32 blen = nf->pcap_le ? gf_bs_read_u32_le(nf->cap_bs) : gf_bs_read_u32(nf->cap_bs);
	nf->link_types[nf->pcap_num_interfaces] = nf->pcap_le ? gf_bs_read_u16_le(nf->cap_bs) : gf_bs_read_u16(nf->cap_bs);
	gf_bs_skip_bytes(nf->cap_bs, blen-10);

	if (nf->link_types[nf->pcap_num_interfaces]>1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[NetCap] Only ethernet and BSD-loopback pcap files are supported, skipping packets\n"));
	}
	nf->pcap_num_interfaces++;
}

#define SKIP_PCAP_PCK \
	if (clen<0) {\
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Corrupted PCAP file, aborting\n"));\
		exit(1);\
	}\
	gf_bs_skip_bytes(nf->cap_bs, clen);\
	clen=0;\
	goto refetch_pcap;


static void gf_netcap_load_pck_pcap(GF_NetcapFilter *nf)
{
	u32 s, frac;
	s32 clen=0, bsize=0;

refetch_pcap:
	nf->dst_port = 0;
	nf->pck_len = 0;
	if (nf->pcapng_trail) {
		gf_bs_skip_bytes(nf->cap_bs, nf->pcapng_trail);
		nf->pcapng_trail = 0;
	} else if (nf->pcap_trail) {
		gf_bs_skip_bytes(nf->cap_bs, nf->pcapng_trail);
		nf->pcap_trail = 0;
	}

	if (gf_bs_available(nf->cap_bs)<16) {
		nf->is_eos = GF_TRUE;
		return;
	}

	u32 link_type;
	if (nf->cap_mode==NETCAP_PCAPNG) {

		u32 btype = nf->pcap_le ? gf_bs_read_u32_le(nf->cap_bs) : gf_bs_read_u32(nf->cap_bs);
		if (btype==GF_4CC(0x0A, 0x0D, 0x0D, 0x0A)) {
			pcapng_load_shb(nf);
			goto refetch_pcap;
		} else if (btype==0x00000001) {
			pcapng_load_idb(nf);
			goto refetch_pcap;
		}

		bsize = nf->pcap_le ? gf_bs_read_u32_le(nf->cap_bs) : gf_bs_read_u32(nf->cap_bs);
		if (btype!=0x00000006) {
			gf_bs_skip_bytes(nf->cap_bs, bsize-8);
			nf->pcapng_trail = 0;
			goto refetch_pcap;
		}
		u32 ifce_idx = nf->pcap_le ? gf_bs_read_u32_le(nf->cap_bs) : gf_bs_read_u32(nf->cap_bs);
		if (ifce_idx<nf->pcap_num_interfaces)
			link_type = nf->link_types[0];
		else
			link_type = 0xFF;
	} else {
		link_type = nf->link_types[0];
	}

	if (nf->pcap_le) {
		s = gf_bs_read_u32_le(nf->cap_bs);
		frac = gf_bs_read_u32_le(nf->cap_bs);
		clen = gf_bs_read_u32_le(nf->cap_bs);
		/*olen = */gf_bs_read_u32_le(nf->cap_bs);
	} else {
		s = gf_bs_read_u32(nf->cap_bs);
		frac = gf_bs_read_u32(nf->cap_bs);
		clen = gf_bs_read_u32(nf->cap_bs);
		/*olen = */gf_bs_read_u32(nf->cap_bs);
	}

	if (nf->cap_mode==NETCAP_PCAPNG) {
		nf->pcapng_trail = bsize - 28 - clen;
	}

	Bool ip_v6=GF_FALSE;
	if (link_type==0) {
		u32 type = nf->pcap_le ? gf_bs_read_u32_le(nf->cap_bs) : gf_bs_read_u32(nf->cap_bs);
		clen-=4;
		if (type==2) {
		} else if ((type==24) || (type==28) || (type==30)) {
			ip_v6=GF_TRUE;
		} else {
			SKIP_PCAP_PCK
		}
	}
	//Ethernet
	else if (link_type==1) {
		gf_bs_skip_bytes(nf->cap_bs, 12);
		u32 etype = gf_bs_read_u16(nf->cap_bs);
		clen-=14;
		if (etype==0x0800) {
		} else if (etype==0x86DD) {
			ip_v6=GF_TRUE;
		} else {
			SKIP_PCAP_PCK
		}
	}
	//not supported
	else {
		gf_bs_skip_bytes(nf->cap_bs, clen);
		goto refetch_pcap;
	}

	if (ip_v6) {
		gf_bs_skip_bytes(nf->cap_bs, 4);
		/*pay_len = */gf_bs_read_u16(nf->cap_bs);
		u32 next_hdr = gf_bs_read_u8(nf->cap_bs);
		gf_bs_read_u8(nf->cap_bs);
		gf_bs_read_data(nf->cap_bs, nf->src_v6, 16);
		gf_bs_read_data(nf->cap_bs, nf->dst_v6, 16);
		clen -= 40;
		Bool has_hdr = 1;
		while (has_hdr) {
			u32 hdr_size;
			switch (next_hdr) {
			case 0:
			case 60:
			case 43:
			case 44:
			case 50:
			case 51:
			case 135:
			case 139:
			case 140:
				next_hdr = gf_bs_read_u8(nf->cap_bs);
				hdr_size = gf_bs_read_u8(nf->cap_bs);
				gf_bs_skip_bytes(nf->cap_bs, 2 + 8*hdr_size);
				hdr_size = 8*(hdr_size+1);
				clen -= hdr_size;
				break;
			case 6:
			case 17:
				has_hdr=0;
				break;
			default:
				SKIP_PCAP_PCK
			}
		}
	} else {
		gf_bs_skip_bytes(nf->cap_bs, 9);
		u8 ptype = gf_bs_read_u8(nf->cap_bs);
		gf_bs_skip_bytes(nf->cap_bs, 2);
		nf->src_v4 = gf_bs_read_u32_le(nf->cap_bs);
		nf->dst_v4 = gf_bs_read_u32_le(nf->cap_bs);
		clen-=20;
		//UDP
		if (ptype==17) {
			nf->src_port = gf_bs_read_u16(nf->cap_bs);
			nf->dst_port = gf_bs_read_u16(nf->cap_bs);
			gf_bs_skip_bytes(nf->cap_bs, 4);
			clen-=8;
		}
		//TCP
		else if (ptype==6) {
			nf->src_port = gf_bs_read_u16(nf->cap_bs);
			nf->dst_port = gf_bs_read_u16(nf->cap_bs);
			/*sn = */gf_bs_read_u32(nf->cap_bs);
			/*ack = */gf_bs_read_u32(nf->cap_bs);
			u32 data_offset = gf_bs_read_int(nf->cap_bs, 4);
			gf_bs_read_int(nf->cap_bs, 4);
			gf_bs_skip_bytes(nf->cap_bs, 7);
			if (data_offset>5)
				gf_bs_skip_bytes(nf->cap_bs, (data_offset-5)*4);
			clen -= data_offset*4;
		} else {
			//no support
			SKIP_PCAP_PCK
		}
	}
	if (clen<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Corrupted PCAP file, aborting\n"));
		exit(1);
	}
	//for pcap, skip ethernet CRC
	if ((nf->cap_mode==NETCAP_PCAP) && (link_type==1))
		nf->pcap_trail = 4;

	if (nf->cap_mode==NETCAP_PCAP) {
		nf->pck_time = s;
		nf->pck_time *= 1000000;
		if (nf->pcap_nano)
			nf->pck_time+=frac/1000;
		else
			nf->pck_time+=frac;
	} else {
		nf->pck_time = s;
		nf->pck_time <<= 32;
		nf->pck_time |= frac;
		//in microsec since UTC epoch
	}
	if (!nf->init_time) {
		nf->init_time = gf_sys_clock_high_res();
		nf->init_time -= nf->pck_time;
	}
	nf->pck_time += nf->init_time;
	nf->pck_len = clen;
	if (!clen) goto refetch_pcap;
}

static void gf_netcap_playback(GF_NetcapFilter *nf);

GF_Err gf_netcap_setup(char *rules)
{
	char *id=NULL;
	char *src=NULL;
	char *dst=NULL;
	s32 cap_loop=0;
	Bool cap_rt=GF_TRUE;
	char *rsep = strchr(rules, '[');
	if (rsep) rsep[0] = 0;
	//extract src or dst, nrt flag and loop flag
	while (rules) {
		char *sep = strchr(rules, ',');
		if (sep) sep[0] = 0;
		if (!strncmp(rules, "id=", 3)) {
			id = gf_strdup(rules+3);
		} else if (!strncmp(rules, "src=", 4)) {
			src = gf_strdup(rules+4);
		} else if (!strncmp(rules, "dst=", 4)) {
			dst = gf_strdup(rules+4);
		} else if (!strcmp(rules, "loop")) {
			cap_loop = -1;
		} else if (!strncmp(rules, "loop=", 5)) {
			cap_loop = atoi(rules+5);
		} else if (!strcmp(rules, "nrt")) {
			cap_rt = GF_FALSE;
		}
		if (!sep) {
			rules=NULL;
			break;
		}
		sep[0] = ',';
		rules = sep+1;
		if (rules[0]=='[') break;
	}
	if (rsep) {
		rsep[0] = '[';
		rules = rsep;
	}
	if (!dst && !src && !rules) {
		if (id) gf_free(id);
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[NetCap] Invalid netcap rule, ignoring\n"));
		return GF_OK;
	}
	if (dst && src) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Invalid netcap, src and dst cannot be set at the same time\n"));
		if (id) gf_free(id);
		gf_free(src);
		gf_free(dst);
		return GF_BAD_PARAM;
	}

	GF_NetcapFilter *nf = gf_net_filter_get(id);
	if (nf) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Several netcap filters with same IDs are not supported, ignoring\n"));
		if (id) gf_free(id);
		if (src) gf_free(src);
		if (dst) gf_free(dst);
		return GF_BAD_PARAM;
	}

	GF_SAFEALLOC(nf, GF_NetcapFilter);
	if (!nf) {
		if (id) gf_free(id);
		if (src) gf_free(src);
		if (dst) gf_free(dst);
		return GF_OUT_OF_MEM;
	}
	nf->id = id;
	nf->src = src;
	nf->dst = dst;
	nf->loops = cap_loop;
	nf->rt = cap_rt;
	nf->rules = gf_list_new();
	if (!nf->rules) {
		if (id) gf_free(id);
		if (src) gf_free(src);
		if (dst) gf_free(dst);
		gf_free(nf);
		return GF_OUT_OF_MEM;
	}
	if (!netcap_filters) netcap_filters = gf_list_new();
	gf_list_add(netcap_filters, nf);

#ifdef GPAC_HAS_FD
	nf->fd = -1;
#endif

	while (rules) {
		u32 send_recv = 1; //default reception only
		u32 port=0;
		u32 num_pck=1;
		u32 pck_start=0;
		u32 pck_end=0;
		s32 patch_offset=-1;
		s32 patch_val=-1;
		s32 rand_every = 0;
		char *rule_str;
		char *sep = strchr(rules, '[');
		if (!sep) break;
		sep = strchr(sep+1, ']');
		if (!sep) break;

		sep[0] = 0;
		rule_str = rules+1;
		while (rule_str[1]=='=') {
			char *sep2 = strchr(rule_str+2, ',');
			if (sep2) sep2[0]=0;

			switch (rule_str[0]) {
			case 'm':
				if (!strcmp(rule_str+2, "r")) send_recv = 1;
				else if (!strcmp(rule_str+2, "w")) send_recv = 2;
				else if (!strcmp(rule_str+2, "rw") || !strcmp(rule_str+2, "wr")) send_recv = 0;
				else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[NetCap] Unknown mode %s, using read/write\n", rule_str+2));
					send_recv=0;
				}
				break;
			case 'p':
				port = atoi(rule_str+2);
				break;
			case 's':
				pck_start = atoi(rule_str+2);
				break;
			case 'e':
				pck_end = atoi(rule_str+2);
				break;
			case 'n':
				num_pck = atoi(rule_str+2);
				break;
			case 'r':
				rand_every = atoi(rule_str+2);
				if (rand_every<0) rand_every = -rand_every;
				break;
			case 'f':
				rand_every = atoi(rule_str+2);
				if (rand_every>0) rand_every = -rand_every;
				break;
			case 'o':
				patch_offset = atoi(rule_str+2);
				break;
			case 'v':
				if (strchr(rule_str, '-')) patch_val =-1;
				else patch_val = strtol(rule_str+2, NULL, 16);
				break;
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[NetCap] Unknown directive %c, ignoring\n", rule_str[0]));
			}

			if (!sep2) break;
			sep2[0] = ',';
			rule_str = sep2+1;
		}

		NetFilterRule *rule;
		GF_SAFEALLOC(rule, NetFilterRule);
		rule->send_recv = send_recv;
		rule->port = port;
		rule->patch_offset = patch_offset;
		rule->patch_val = patch_val;
		rule->pck_start = pck_start;
		rule->pck_end = pck_end>pck_start ? pck_end : 0;
		rule->rand_every = rand_every;
		rule->nb_pck = num_pck;

		gf_list_add(nf->rules, rule);
		sep[0] = ']';
		rules = sep+1;
	}
	nf->nb_rules = gf_list_count(nf->rules);

	if (nf->dst) gf_netcap_record(nf);
	if (nf->src) gf_netcap_playback(nf);

	return GF_OK;
}

static Bool netcap_filter_pck(GF_Socket *sock, u32 pck_len, Bool for_send)
{
	u32 i=0;
	if (!sock || !sock->cap_info) return GF_FALSE;
	GF_NetcapFilter *nf = sock->cap_info->nf;

	if (for_send) {
		nf->pck_idx_w++;
		sock->cap_info->pck_idx_w++;
	} else {
		nf->pck_idx_r++;
		sock->cap_info->pck_idx_r++;
	}

	for (i=0; i<nf->nb_rules; i++) {
		NetFilterRule *r = gf_list_get(nf->rules, i);
		if (r->port && (r->port != nf->dst_port)) continue;
		if (for_send) {
			if (r->send_recv==1) continue;
		} else {
			if (r->send_recv==2) continue;
		}
		NetCapInfo *ci = r->port ? sock->cap_info : NULL;
		u32 cur_pck;
		if (ci) {
			cur_pck = for_send ? ci->pck_idx_w : ci->pck_idx_r;
		} else {
			cur_pck = for_send ? nf->pck_idx_w : nf->pck_idx_r;
		}
		//check range
		if (r->pck_start && (cur_pck < r->pck_start)) continue;
		if (r->pck_end && (cur_pck >= r->pck_end)) continue;

		//not repeated pattern case
		if (!r->rand_every) {
			if ((cur_pck >= r->pck_start) && (r->pck_start + r->nb_pck > cur_pck)) {}
			else continue;
		}
		//repeated pattern case
		else {
			u32 next_pck_range = ci ? ci->next_pck_range : nf->next_pck_range;
			u32 next_rand = ci ? ci->next_rand : nf->next_rand;
			//in range, check if we need to recompute
			if (next_pck_range < r->pck_start)
				next_pck_range = r->pck_start;

			if (next_pck_range <= cur_pck) {
				if (r->rand_every>0) {
					next_rand = next_pck_range + gf_rand() % r->rand_every;
					next_pck_range += r->rand_every;
				} else {
					next_rand = next_pck_range;
					next_pck_range += -r->rand_every;
				}

				if (ci) {
					ci->next_pck_range = next_pck_range;
					ci->next_rand = next_rand;
				} else {
					nf->next_pck_range = next_pck_range;
					nf->next_rand = next_rand;
				}
			}

			//ours
			if (r->nb_pck) {
				if ((next_rand<=cur_pck) && (next_rand+r->nb_pck>cur_pck)) {}
				else continue;
			} else {
				if (next_rand==cur_pck) {}
				else continue;
			}
		}

		if ((r->patch_offset==-1) && !(sock->flags & GF_SOCK_IS_TCP)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[NetCap] Droping packet %d\n", cur_pck));
			return GF_TRUE;
		}
		u32 bo = (r->patch_offset>=0) ? r->patch_offset : gf_rand();
		bo = bo % pck_len;
		sock->cap_info->patch_offset = 1+bo;

		u32 val = (r->patch_val>=0) ? r->patch_val : gf_rand();
		val = val % 256;

		sock->cap_info->patch_val = val;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[NetCap] Patching packet %d byte offset %d to 0x%02X\n", cur_pck, bo, val));
		// we want to patch many bytes of same packet
		// return GF_FALSE;
	}
	return GF_FALSE;
}

static void gf_netcap_load_pck(GF_NetcapFilter *nf, u64 now)
{

refetch:
	//no packet loaded
	if (!nf->dst_port) {
		nf->pck_flags=0;
		nf->read_sock_selected = NULL;

		if (nf->loops && !gf_bs_available(nf->cap_bs)) {
			if (nf->loops>0)
				nf->loops--;

			gf_bs_seek(nf->cap_bs, 0);
			nf->init_time = 0;
			if (nf->cap_mode==NETCAP_PCAP)
				gf_bs_skip_bytes(nf->cap_bs, 24);
		}

		if (nf->cap_mode==NETCAP_GPAC)
			gf_netcap_load_pck_gpac(nf);
		else
			gf_netcap_load_pck_pcap(nf);

		//eof
		if (!nf->dst_port) return;
		//force discard
		if ((nf->pck_flags & NETCAP_DROP)) {
			gf_bs_skip_bytes(nf->cap_bs, nf->pck_len);
			nf->dst_port = 0;
			goto refetch;
		}
	}

	if (nf->rt && now && (nf->pck_time > now)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[NetCap] Packet too early by %d ms\n", (u32) (nf->pck_time - now) / 1000));
		return;
	}

	//we already have the socket
	if (nf->read_sock_selected)
		return;

	//find destination socket
	u32 i=0;
	GF_Socket *s = NULL;
	while ((s=gf_list_enum(nf->read_socks, &i))) {
		if (s->cap_info->host_port != nf->dst_port)
			continue;
		if (nf->pck_flags & NETCAP_IS_IPV6) {
			if (memcmp(s->cap_info->host_addr_v6, nf->dst_v6, 16)) continue;
		} else {
			//accept INADDR_ANY
			if (s->cap_info->host_addr_v4 && (s->cap_info->host_addr_v4 != nf->dst_v4)) continue;
			if (s->flags & GF_SOCK_IS_TCP) {
				if (s->cap_info->peer_addr_v4 != nf->src_v4) continue;
				if (!s->cap_info->peer_port) {
					s->cap_info->peer_port = nf->src_port;
				} else if (s->cap_info->peer_port != nf->src_port) {
					continue;
				}
			}
		}
		nf->read_sock_selected = s;
		s->cap_info->patch_offset = 0;
		break;
	}
	if (netcap_filter_pck(nf->read_sock_selected, nf->pck_len, GF_FALSE))
		nf->read_sock_selected = NULL;

	if (!nf->read_sock_selected) {
		gf_bs_skip_bytes(nf->cap_bs, nf->pck_len);
		nf->dst_port = 0;
		goto refetch;
	}
}

static void gf_netcap_playback(GF_NetcapFilter *nf)
{
#ifdef GPAC_HAS_FD
	if (!gf_opts_get_bool("core", "no-fd")) {
		nf->fd = open(nf->src, O_RDONLY);
		if (nf->fd>=0) nf->cap_bs = gf_bs_from_fd(nf->fd, GF_BITSTREAM_READ);
	} else
#endif
	{
		nf->file = gf_fopen_ex(nf->src, NULL, "r", GF_FALSE);
		if (nf->file) nf->cap_bs = gf_bs_from_file(nf->file, GF_BITSTREAM_READ);
	}
	if (!nf->cap_bs) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Failed to setup capture playback\n"));
		exit(1);
	}
	nf->read_socks = gf_list_new();

	u32 magic = gf_bs_read_u32(nf->cap_bs);
	if (magic == GF_GPC_MAGIC) {
		nf->cap_mode = NETCAP_GPAC;
	} else if (magic==GF_4CC(0xD4, 0xC3, 0xB2, 0xA1)) {
		nf->cap_mode = NETCAP_PCAP;
		nf->pcap_le = GF_TRUE;
	} else if (magic==GF_4CC(0xA1, 0xB2, 0xC3, 0xD4)) {
		nf->cap_mode = NETCAP_PCAP;
	} else if (magic==GF_4CC(0x4D, 0x3C, 0xB2, 0xA1)) {
		nf->cap_mode = NETCAP_PCAP;
		nf->pcap_nano = GF_TRUE;
		nf->pcap_le = GF_TRUE;
	} else if (magic==GF_4CC(0xA1, 0xB2, 0x3C, 0x4D)) {
		nf->cap_mode = NETCAP_PCAP;
		nf->pcap_nano = GF_TRUE;
	} else if (magic==GF_4CC(0x0A, 0x0D, 0x0D, 0x0A)) {
		nf->cap_mode = NETCAP_PCAPNG;
		nf->pcap_nano = GF_TRUE;
	} else {
		gf_net_close_capture();
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Unsupported capture file magic %s\n", gf_4cc_to_str(magic)));
		exit(1);
	}
	if (nf->cap_mode==NETCAP_GPAC) {
		/*ntp_sec = */gf_bs_read_u32(nf->cap_bs);
		/*ntp_frac = */gf_bs_read_u32(nf->cap_bs);
		magic = gf_bs_read_u32(nf->cap_bs);
		gf_bs_skip_bytes(nf->cap_bs, magic);
		return;
	}

	if (nf->cap_mode==NETCAP_PCAP) {
		gf_bs_skip_bytes(nf->cap_bs, 16); //we ignore snapLen
		gf_bs_read_int(nf->cap_bs, 4);
		nf->link_types[0] = gf_bs_read_int(nf->cap_bs, 28);
		if (nf->pcap_le) nf->link_types[0] = ntohl(nf->link_types[0]);

		if (nf->link_types[0] > 1) {
			gf_net_close_capture();
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Only ethernet and BSD-loopback pcap files are supported\n"));
			exit(1);
		}
	}
	//pcapng
	else if (nf->cap_mode==NETCAP_PCAPNG) {
		pcapng_load_shb(nf);

		u32 btype = nf->pcap_le ? gf_bs_read_u32_le(nf->cap_bs) : gf_bs_read_u32(nf->cap_bs);
		if (btype != 1) {
			gf_net_close_capture();
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[NetCap] Missing IDB block after pacpng header\n"));
			exit(1);
		}
		pcapng_load_idb(nf);
	}
}

static GF_Err gf_netcap_send(GF_Socket *sock, const u8 *buffer, u32 length, u32 *written)
{
	GF_NetcapFilter *nf = sock->cap_info->nf;
	u64 now = gf_sys_clock_high_res();
	if (!sock->cap_info->host_port) return GF_BAD_PARAM;
	if (!nf->init_time) nf->init_time = now;
	now -= nf->init_time;

	//setup flags and size flags
	u32 size = 1+8+2+length;
	u8 flags=0;
	if (sock->flags & GF_SOCK_IS_MULTICAST) flags |= NETCAP_IS_MCAST;
	if (sock->flags & GF_SOCK_IS_TCP) flags |= NETCAP_IS_TCP;
	if (sock->flags & GF_SOCK_HAS_PEER) flags |= NETCAP_HAS_PEER;
	if (sock->flags & GF_SOCK_IS_IPV6) {
		size += 16;
		if (sock->flags & GF_SOCK_HAS_PEER) {
			size += 18;
		}
		flags |= NETCAP_IS_IPV6;
	} else {
		size += 4;
		if (sock->flags & GF_SOCK_HAS_PEER) {
			size += 6;
		}
	}
	gf_bs_write_u32(nf->cap_bs, size);
	gf_bs_write_u8(nf->cap_bs, flags);
	gf_bs_write_u64(nf->cap_bs, now);
	if (sock->flags & GF_SOCK_IS_IPV6) {
		gf_bs_write_data(nf->cap_bs, sock->cap_info->host_addr_v6, 16);
		if (sock->flags & GF_SOCK_HAS_PEER)
			gf_bs_write_data(nf->cap_bs, sock->cap_info->peer_addr_v6, 16);
	} else {
		gf_bs_write_u32(nf->cap_bs, sock->cap_info->host_addr_v4);
		if (sock->flags & GF_SOCK_HAS_PEER)
			gf_bs_write_u32(nf->cap_bs, sock->cap_info->peer_addr_v4);
	}
	gf_bs_write_u16(nf->cap_bs, sock->cap_info->host_port);
	if (sock->flags & GF_SOCK_HAS_PEER)
		gf_bs_write_u16(nf->cap_bs, sock->cap_info->peer_port);
	gf_bs_write_data(nf->cap_bs, buffer, length);
	if (written) *written = length;
	return GF_OK;
}

#endif //GPAC_DISABLE_NETCAP


GF_EXPORT
GF_Socket *gf_sk_new_ex(u32 SocketType, const char *netcap_id)
{
	GF_Socket *tmp;

	/*init WinSock*/
#ifdef WIN32
	WSADATA Data;
	if (!wsa_init && (WSAStartup(0x0202, &Data)!=0) ) return NULL;
#endif
	switch (SocketType) {
	case GF_SOCK_TYPE_UDP:
	case GF_SOCK_TYPE_TCP:
#ifdef GPAC_HAS_SOCK_UN
	case GF_SOCK_TYPE_UDP_UN:
	case GF_SOCK_TYPE_TCP_UN:
#endif
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Socket] unsupported socket type %d\n", SocketType));
		return NULL;
	}

	GF_SAFEALLOC(tmp, GF_Socket);
	if (!tmp) return NULL;
	if (SocketType == GF_SOCK_TYPE_TCP) tmp->flags |= GF_SOCK_IS_TCP;
#ifdef GPAC_HAS_SOCK_UN
	else if (SocketType == GF_SOCK_TYPE_TCP_UN) tmp->flags |= GF_SOCK_IS_TCP | GF_SOCK_IS_UN;
	else if (SocketType == GF_SOCK_TYPE_UDP_UN) tmp->flags |= GF_SOCK_IS_UN;
#endif

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

#ifndef GPAC_DISABLE_NETCAP
	GF_NetcapFilter *nf = gf_net_filter_get(netcap_id);
	if (nf && (nf->cap_bs || nf->nb_rules)) {
		GF_SAFEALLOC(tmp->cap_info, NetCapInfo);
		if (!tmp->cap_info) {
			gf_free(tmp);
			return NULL;
		}
		if (nf->read_socks)
			gf_list_add(nf->read_socks, tmp);
		tmp->cap_info->nf = nf;
	}
#endif
	return tmp;
}

GF_EXPORT
GF_Socket *gf_sk_new(u32 SocketType)
{
	return gf_sk_new_ex(SocketType, NULL);
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
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Socket] Couldn't set socket %s buffer size to %d: %s (%d)\n", SendBuffer ? "send" : "receive", NewSize, gf_errno_str(LASTSOCKERROR), LASTSOCKERROR));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[Socket] Set socket %s buffer size to %d\n", SendBuffer ? "send" : "receive", NewSize));
	}
	psize = sizeof(u32);
	if (SendBuffer) {
		res = getsockopt(sock->socket, SOL_SOCKET, SO_SNDBUF, (char *) &nsize, &psize );
	} else {
		res = getsockopt(sock->socket, SOL_SOCKET, SO_RCVBUF, (char *) &nsize, &psize );
	}
	if ((res>=0) && (nsize!=NewSize)) {
		GF_LOG(nsize <= NewSize ? GF_LOG_WARNING : GF_LOG_DEBUG, GF_LOG_NETWORK, ("[Socket] Asked to set socket %s buffer size to %d but system used %d\n", SendBuffer ? "send" : "receive", NewSize, nsize));
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
	if (sock->socket) {
		s32 flags = fcntl(sock->socket, F_GETFL, 0);
		if (NonBlockingOn)
			flags |= O_NONBLOCK;
		else
			flags &= ~O_NONBLOCK;

		res = fcntl(sock->socket, F_SETFL, flags);
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

#ifndef GPAC_DISABLE_NETCAP
#define sock_close(_s) \
	closesocket(_s->socket);\
	_s->socket = NULL_SOCKET;\
	if (_s->cap_info) _s->cap_info->host_port = 0; \

#else

#define sock_close(_s) \
	closesocket(_s->socket);\
	_s->socket = NULL_SOCKET;

#endif

static void gf_sk_free(GF_Socket *sock)
{
	gf_assert( sock );
	if (!sock->socket) return;

	/*leave multicast*/
	if (sock->flags & GF_SOCK_IS_MULTICAST) {
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

	sock_close(sock);
}


GF_EXPORT
void gf_sk_del(GF_Socket *sock)
{
	gf_assert( sock );
	gf_sk_free(sock);
#ifdef WIN32
	wsa_init --;
	if (!wsa_init) WSACleanup();
#endif

#ifndef GPAC_DISABLE_NETCAP
	if (sock->cap_info) {
		if (sock->cap_info->nf->read_socks)
			gf_list_del_item(sock->cap_info->nf->read_socks, sock);
		gf_free(sock->cap_info);
	}
#endif
	gf_free(sock);
}

GF_EXPORT
void gf_sk_reset(GF_Socket *sock)
{
	if (sock) {
		u32 clear=0;
		setsockopt(sock->socket, SOL_SOCKET, SO_ERROR, (char *) &clear, sizeof(u32) );
	}
}

GF_EXPORT
s32 gf_sk_get_handle(GF_Socket *sock)
{
	return (s32) sock->socket;
}

GF_EXPORT
void gf_sk_set_usec_wait(GF_Socket *sock, u32 usec_wait)
{
	if (!sock) return;
	sock->usec_wait = (usec_wait>=1000000) ? 500 : usec_wait;
}

#ifdef GPAC_STATIC_BIN
struct hostent *gf_gethostbyname(const char *PeerName)
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("Static GPAC build has no DNS support, cannot resolve host %s !\n", PeerName));
	return NULL;
}
#else
#define gf_gethostbyname gethostbyname
#endif

static u32 inet_addr_from_name(const char *local_interface);

//connects a socket to a remote peer on a given port
GF_EXPORT
GF_Err gf_sk_connect(GF_Socket *sock, const char *PeerName, u16 PortNumber, const char *ifce_ip_or_name)
{
	s32 ret;
#ifdef GPAC_HAS_IPV6
	u32 type;
	struct addrinfo *res, *aip, *lip;
#else
	struct hostent *Host = NULL;
#endif

	if (sock->flags & GF_SOCK_IS_UN) {
#ifdef GPAC_HAS_SOCK_UN
		struct sockaddr_un server_add;
		if (!sock->socket) {
			sock->socket = socket(AF_UNIX, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
			if (sock->flags & GF_SOCK_NON_BLOCKING)
				gf_sk_set_block_mode(sock, GF_TRUE);
		}
		server_add.sun_family = AF_UNIX;
		strncpy(server_add.sun_path, PeerName, sizeof(server_add.sun_path)-1);
		server_add.sun_path[sizeof(server_add.sun_path)-1]=0;
		if (connect(sock->socket, (struct sockaddr *)&server_add, sizeof(struct sockaddr_un)) < 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Socket] Failed to connect unix domain socket to %s\n", PeerName));
			return GF_IP_CONNECTION_FAILURE;
		}
		return GF_OK;
#else
	     return GF_NOT_SUPPORTED;
#endif
	}

#ifdef GPAC_HAS_IPV6
	type = (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM;

	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Solving %s address\n", PeerName));
	res = gf_sk_get_ipv6_addr(PeerName, PortNumber, AF_UNSPEC, AI_PASSIVE, type);
	if (!res) return GF_IP_CONNECTION_FAILURE;
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Host %s found\n", PeerName));

	lip = NULL;
	if (ifce_ip_or_name) {
		lip = gf_sk_get_ifce_ipv6_addr(ifce_ip_or_name, PortNumber, AF_UNSPEC, AI_PASSIVE, type);
		if (!lip) return GF_IP_CONNECTION_FAILURE;
	}

	/*for all interfaces*/
	for (aip=res; aip!=NULL; aip=aip->ai_next) {
		if (type != (u32) aip->ai_socktype) continue;


#ifndef GPAC_DISABLE_NETCAP
		if (sock->cap_info) {
			sock->cap_info->host_port = PortNumber;
			if (lip) {
				if (lip->ai_family==PF_INET) {
					sock->cap_info->host_addr_v4 = inet_addr_from_name(ifce_ip_or_name);
				} else {
					memcpy(sock->cap_info->host_addr_v6, (u8*) &((struct sockaddr_in6 *)lip->ai_addr)->sin6_addr, sizeof(bin128));
				}
			}
			if (aip->ai_family==AF_INET) {
				sock->cap_info->peer_addr_v4 = inet_addr_from_name(PeerName);
				sock->cap_info->peer_addr_v4 = ((struct sockaddr_in *)aip->ai_addr)->sin_addr.s_addr;
			} else {
				memcpy(sock->cap_info->peer_addr_v6, (u8*) &((struct sockaddr_in6 *)aip->ai_addr)->sin6_addr, sizeof(bin128));
			}
			//play/record, don't create socket
			if (sock->cap_info->nf->cap_mode) {
				if (sock->cap_info->nf->read_socks==NULL) {
					sock->cap_info->peer_port = first_tcp_port;
					first_tcp_port++;
				}
				sock->flags |= GF_SOCK_HAS_PEER;
				freeaddrinfo(res);
				if (lip) freeaddrinfo(lip);
				return GF_OK;
			}
		}
#endif
		if (!sock->socket) {
			sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
			if (sock->socket == INVALID_SOCKET) {
				sock->socket = NULL_SOCKET;
				continue;
			}
			if (sock->flags & GF_SOCK_IS_TCP) {
				if (sock->flags & GF_SOCK_NON_BLOCKING)
					gf_sk_set_block_mode(sock, GF_TRUE);
			}

			if (aip->ai_family==PF_INET6) sock->flags |= GF_SOCK_IS_IPV6;
			else sock->flags &= ~GF_SOCK_IS_IPV6;

			if (lip) {
				ret = bind(sock->socket, lip->ai_addr, (int) lip->ai_addrlen);
				if (ret == SOCKET_ERROR) {
					sock_close(sock);
					continue;
				}
			}
		}

		if (sock->flags & GF_SOCK_IS_TCP) {

#if defined(WIN32) || defined(_WIN32_WCE)
			//on winsock we must check writability between two connects for non-blocking sockets
			if (sock->flags & GF_SOCK_HAS_CONNECT) {
				if (gf_sk_select(sock, GF_SK_SELECT_WRITE) == GF_IP_NETWORK_EMPTY) {
					GF_Err e = gf_sk_probe(sock);
					if (e && (e != GF_IP_NETWORK_EMPTY)) return e;
					return GF_IP_NETWORK_EMPTY;
				}
			}
#endif

			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Connecting to %s:%d\n", PeerName, PortNumber));
			ret = connect(sock->socket, aip->ai_addr, (int) aip->ai_addrlen);
			if (ret == SOCKET_ERROR) {
				int err = LASTSOCKERROR;
				if (sock->flags & GF_SOCK_NON_BLOCKING) {
					switch (err) {
					case EINPROGRESS:
#if defined(WIN32) || defined(_WIN32_WCE)
					case WSAEWOULDBLOCK:
#endif
						freeaddrinfo(res);
						if (lip) freeaddrinfo(lip);
						//remember we issued a first connect
						sock->flags |= GF_SOCK_HAS_CONNECT;
						return GF_IP_NETWORK_EMPTY;

					case EISCONN:
					case EALREADY:
#if defined(WIN32) || defined(_WIN32_WCE)
					case WSAEISCONN:
#endif
						if (gf_sk_select(sock, GF_SK_SELECT_WRITE) == GF_OK)
							goto conn_ok;
						freeaddrinfo(res);
						if (lip) freeaddrinfo(lip);
						return GF_IP_NETWORK_EMPTY;
					}
				}
				sock_close(sock);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[Sock_IPV6] Failed to connect to host %s: %s (%d) - retrying with next host address\n", PeerName, gf_errno_str(err), err ));
				continue;
			}
conn_ok:
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV6] Connected to %s:%d\n", PeerName, PortNumber));
			sock->flags &= ~GF_SOCK_HAS_CONNECT;

#ifdef SO_NOSIGPIPE
			int value = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
#endif
		}
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

#ifndef GPAC_DISABLE_NETCAP
	if (sock->cap_info) {
		sock->cap_info->host_port = PortNumber;
		if (ifce_ip_or_name)
			sock->cap_info->host_addr_v4 = inet_addr_from_name(ifce_ip_or_name);

		sock->cap_info->peer_addr_v4 = inet_addr_from_name(PeerName);
		if (sock->cap_info->nf->read_socks==NULL) {
			sock->cap_info->peer_port = first_tcp_port;
			first_tcp_port++;
		}
		
		//play/record, don't create socket
		if (sock->cap_info->nf->cap_mode) {
			sock->flags |= GF_SOCK_HAS_PEER;
			return GF_OK;
		}
	}
#endif

	if (!sock->socket) {
		sock->socket = socket(AF_INET, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
		if (sock->flags & GF_SOCK_NON_BLOCKING)
			gf_sk_set_block_mode(sock, GF_TRUE);
	}

	/*setup the address*/
	sock->dest_addr.sin_family = AF_INET;
	sock->dest_addr.sin_port = htons(PortNumber);
	/*get the server IP*/
	sock->dest_addr.sin_addr.s_addr = inet_addr(PeerName);
	if (sock->dest_addr.sin_addr.s_addr==INADDR_NONE) {
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Solving %s address\n", PeerName));
		Host = gf_gethostbyname(PeerName);
		if (Host == NULL) {
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Failed to retrieve host %s address: %s\n", PeerName, gf_errno_str(LASTSOCKERROR) ));
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
		memcpy(&sock->dest_addr.sin_addr, Host->h_addr_list[0], sizeof(u8)*Host->h_length);
	}

	if (ifce_ip_or_name) {
		GF_Err e = gf_sk_bind(sock, ifce_ip_or_name, PortNumber, PeerName, PortNumber, GF_SOCK_REUSE_PORT);
		if (e) return e;
	}
	if (!(sock->flags & GF_SOCK_IS_TCP)) {
		return GF_OK;
	}

#if defined(WIN32) || defined(_WIN32_WCE)
	//on winsock we must check writability between two connects for non-blocking sockets
	if (sock->flags & GF_SOCK_HAS_CONNECT) {
		if (gf_sk_select(sock, GF_SK_SELECT_WRITE) == GF_IP_NETWORK_EMPTY)
			return GF_IP_NETWORK_EMPTY;
	}
#endif

	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Connecting to %s:%d\n", PeerName, PortNumber));
	ret = connect(sock->socket, (struct sockaddr *) &sock->dest_addr, sizeof(struct sockaddr));
	if (ret == SOCKET_ERROR) {
		u32 res = LASTSOCKERROR;

		if (sock->flags & GF_SOCK_NON_BLOCKING) {
			switch (res) {
			case EINPROGRESS:
#if defined(WIN32) || defined(_WIN32_WCE)
			case WSAEWOULDBLOCK:
#endif
				//remember we issued a first connect
				sock->flags |= GF_SOCK_HAS_CONNECT;
				return GF_IP_NETWORK_EMPTY;
			case EISCONN:
			case EALREADY:
#if defined(WIN32) || defined(_WIN32_WCE)
			case WSAEISCONN:
#endif
				if (gf_sk_select(sock, GF_SK_SELECT_WRITE) == GF_OK)
					return GF_OK;
				return GF_IP_NETWORK_EMPTY;
			}
		}
		sock->flags &= ~GF_SOCK_HAS_CONNECT;

		switch (res) {
		case EAGAIN:
#if defined(WIN32) || defined(_WIN32_WCE)
		case WSAEWOULDBLOCK:
#endif
			return GF_IP_NETWORK_EMPTY;
#ifdef WIN32
		case WSAEINVAL:
			if (sock->flags & GF_SOCK_NON_BLOCKING)
				return GF_IP_NETWORK_EMPTY;
#endif
		case EISCONN:
			return GF_OK;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Sock_IPV4] Couldn't connect socket: %s\n", gf_errno_str(res)));
			return GF_IP_CONNECTION_FAILURE;
		}
	}
	sock->flags &= ~GF_SOCK_HAS_CONNECT;
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[Sock_IPV4] Connected to %s:%d\n", PeerName, PortNumber));

#endif
	return GF_OK;
}

//binds the given socket to the specified port. If ReUse is true
//this will enable reuse of ports on a single machine
GF_EXPORT
GF_Err gf_sk_bind(GF_Socket *sock, const char *ifce_ip_or_name, u16 port, const char *peer_name, u16 peer_port, u32 options)
{
#ifdef GPAC_HAS_IPV6
	struct addrinfo *res, *aip;
	int af;
	u32 type;
#else
	u32 ip_add;
	size_t addrlen;
	struct sockaddr_in LocalAdd;
	struct hostent *Host = NULL;
#endif
	s32 ret = 0;
	s32 optval;

	if (!sock || sock->socket) return GF_BAD_PARAM;
	if (ifce_ip_or_name && !strcmp(ifce_ip_or_name, "127.0.0.1"))
		ifce_ip_or_name = NULL;

	if (sock->flags & GF_SOCK_IS_UN) {
#ifdef GPAC_HAS_SOCK_UN
		struct sockaddr_un server_un;
		if (!sock->socket) {
			sock->socket = socket(AF_UNIX, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
			if (sock->flags & GF_SOCK_NON_BLOCKING)
				gf_sk_set_block_mode(sock, GF_TRUE);
		}
		server_un.sun_family = AF_UNIX;
		strncpy(server_un.sun_path, peer_name, sizeof(server_un.sun_path)-1);
		server_un.sun_path[sizeof(server_un.sun_path)-1]=0;
		ret = bind(sock->socket, (struct sockaddr *) &server_un, (int) sizeof(struct sockaddr_un));
		if (ret == SOCKET_ERROR) {
			if (LASTSOCKERROR == EADDRINUSE) {
				return gf_sk_connect(sock, peer_name, peer_port, NULL);
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] cannot bind socket: %s\n", gf_errno_str(LASTSOCKERROR) ));
			return GF_IP_CONNECTION_FAILURE;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[socket] socket bound to unix domain %s\n", peer_name));
		return GF_OK;
#else
	    return GF_NOT_SUPPORTED;
#endif
	}


#ifndef WIN32
	if (!ifce_ip_or_name && (!peer_name || !strcmp(peer_name,"localhost"))) {
		peer_name="127.0.0.1";
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

	res = gf_sk_get_ifce_ipv6_addr(ifce_ip_or_name, port, af, AI_PASSIVE, type);
	if (!res) return GF_IP_ADDRESS_NOT_FOUND;

	/*for all interfaces*/
	for (aip=res; aip!=NULL; aip=aip->ai_next) {
		if (type != (u32) aip->ai_socktype) continue;

		if (aip->ai_next && (aip->ai_next->ai_family==PF_INET) && !gf_net_is_ipv6(peer_name)) continue;

#ifndef GPAC_DISABLE_NETCAP
		if (sock->cap_info) {
			sock->cap_info->host_port = port ? port : peer_port;
			if (aip->ai_family==PF_INET) {
				sock->cap_info->host_addr_v4 = inet_addr_from_name(peer_name ? peer_name : ifce_ip_or_name);
			} else {
				memcpy(sock->cap_info->host_addr_v6, (u8*) &((struct sockaddr_in6 *)aip->ai_addr)->sin6_addr, sizeof(bin128));
			}
			if (peer_name && peer_port) {
				if (sock->dest_addr.ss_family==AF_INET) {
					struct sockaddr_in *r_add = (struct sockaddr_in *) &sock->dest_addr;
					sock->cap_info->peer_port = r_add->sin_port;
					sock->cap_info->peer_addr_v4 = r_add->sin_addr.s_addr;
				} else {
					struct sockaddr_in6 *r_add = (struct sockaddr_in6 *) &sock->dest_addr;
					sock->cap_info->peer_port = r_add->sin6_port;
					memcpy(sock->cap_info->peer_addr_v6, &(r_add->sin6_addr), sizeof(bin128));
				}
			}
			//play/record, don't create socket
			if (sock->cap_info->nf->cap_mode) {
				if (aip->ai_family==PF_INET6) sock->flags |= GF_SOCK_IS_IPV6;
				else sock->flags &= ~GF_SOCK_IS_IPV6;

				freeaddrinfo(res);
				return GF_OK;
			}
		}
#endif
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

		if (sock->flags & GF_SOCK_NON_BLOCKING)
			gf_sk_set_block_mode(sock, GF_TRUE);

		if (peer_name && peer_port)
			sock->flags |= GF_SOCK_HAS_PEER;

		if (! (options & GF_SOCK_FAKE_BIND) ) {
			ret = bind(sock->socket, aip->ai_addr, (int) aip->ai_addrlen);
			if (ret == SOCKET_ERROR) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] bind failed: %s\n", gf_errno_str(LASTSOCKERROR) ));
				sock_close(sock);
				continue;
			}
		}
		if (aip->ai_family==PF_INET6) sock->flags |= GF_SOCK_IS_IPV6;
		else sock->flags &= ~GF_SOCK_IS_IPV6;

		freeaddrinfo(res);
		return GF_OK;
	}
	freeaddrinfo(res);
	GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[Socket] Cannot bind to ifce %s port %d\n", ifce_ip_or_name ? ifce_ip_or_name : "any", port));
	return GF_IP_CONNECTION_FAILURE;

#else

#ifndef GPAC_DISABLE_NETCAP
	if (sock->cap_info) {
		sock->cap_info->host_port = port ? port : peer_port;
		sock->cap_info->host_addr_v4 = inet_addr_from_name(ifce_ip_or_name);
		if (peer_name && peer_port) {
			sock->cap_info->peer_port = peer_port;
			sock->cap_info->peer_addr_v4 = inet_addr_from_name(peer_name);
			sock->flags |= GF_SOCK_HAS_PEER;
		}
		//play/record, don't create socket
		if (sock->cap_info->nf->cap_mode) {
			return GF_OK;
		}
	}
#endif

	sock->socket = socket(AF_INET, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock->flags & GF_SOCK_NON_BLOCKING)
		gf_sk_set_block_mode(sock, GF_TRUE);
	sock->flags &= ~GF_SOCK_IS_IPV6;

	memset((void *) &LocalAdd, 0, sizeof(LocalAdd));

	/*setup the address*/
	ip_add = inet_addr_from_name(ifce_ip_or_name);
	if (ip_add==0xFFFFFFFF) return GF_IP_CONNECTION_FAILURE;

	if (peer_name && peer_port) {
#ifdef WIN32
		if ((inet_addr(peer_name)== ip_add) || !strcmp(peer_name, "127.0.0.1") ) {
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

	if (! (options & GF_SOCK_FAKE_BIND) ) {
		/*bind the socket*/
		ret = bind(sock->socket, (struct sockaddr *) &LocalAdd, (int) addrlen);
		if (ret == SOCKET_ERROR) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] cannot bind socket: %s\n", gf_errno_str(LASTSOCKERROR) ));
			ret = GF_IP_CONNECTION_FAILURE;
		}
	}

	if (peer_name && peer_port) {
		sock->dest_addr.sin_port = htons(peer_port);
		sock->dest_addr.sin_family = AF_INET;
		sock->dest_addr.sin_addr.s_addr = inet_addr(peer_name);
		if (sock->dest_addr.sin_addr.s_addr == INADDR_NONE) {
			Host = gf_gethostbyname(peer_name);
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

Bool gpac_use_poll=GF_TRUE;

static GF_Err poll_select(GF_Socket *sock, GF_SockSelectMode mode, u32 usec, Bool force_select)
{
#ifndef __SYMBIAN32__
	int ready;
	struct timeval timeout;

#ifdef GPAC_HAS_POLL
	if (!force_select && gpac_use_poll) {
		struct pollfd pfd;
		pfd.fd = sock->socket;
		pfd.revents = 0;
		if (mode == GF_SK_SELECT_WRITE) pfd.events = POLLOUT;
		else if (mode == GF_SK_SELECT_READ) pfd.events = POLLIN;
		else pfd.events = POLLIN|POLLOUT;

		ready = poll(&pfd, 1, usec/1000);
		if (ready<0) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_NETWORK_EMPTY;
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot poll: %s\n", gf_errno_str(LASTSOCKERROR) ));
				return GF_IP_NETWORK_FAILURE;
			}
		}
		if (!ready)
			return GF_IP_NETWORK_EMPTY;

		if ((mode == GF_SK_SELECT_WRITE) && !(pfd.revents & POLLOUT))
			return GF_IP_NETWORK_EMPTY;
		if ((mode == GF_SK_SELECT_READ) && !(pfd.revents & POLLIN))
			return GF_IP_NETWORK_EMPTY;

		return GF_OK;
	}
#endif

	fd_set rgroup, wgroup;

	//can we write?
	FD_ZERO(&rgroup);
	FD_ZERO(&wgroup);
	if (mode != GF_SK_SELECT_WRITE)
		FD_SET(sock->socket, &rgroup);
	if (mode != GF_SK_SELECT_READ)
		FD_SET(sock->socket, &wgroup);
	timeout.tv_sec = 0;
	timeout.tv_usec = usec;

	ready = select((int) sock->socket+1,
		(mode != GF_SK_SELECT_WRITE) ? &rgroup : NULL,
		(mode != GF_SK_SELECT_READ) ? &wgroup : NULL,
		 NULL, &timeout);

	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_NETWORK_EMPTY;
		default:
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[socket] select failure: %s\n", gf_errno_str(LASTSOCKERROR)));
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!ready)
		return GF_IP_NETWORK_EMPTY;
	if ((mode == GF_SK_SELECT_WRITE) && !FD_ISSET(sock->socket, &wgroup))
		return GF_IP_NETWORK_EMPTY;
	if ((mode == GF_SK_SELECT_READ) && !FD_ISSET(sock->socket, &rgroup))
		return GF_IP_NETWORK_EMPTY;
	//if mode is both and we are ready, let's go
	return GF_OK;
#endif
}

//send length bytes of a buffer
GF_EXPORT
GF_Err gf_sk_send_ex(GF_Socket *sock, const u8 *buffer, u32 length, u32 *written)
{
	u32 count;
	s32 res;

	if (written) *written = 0;
#ifndef GPAC_DISABLE_NETCAP
	if (netcap_filter_pck(sock, length, GF_TRUE)) {
		if (written) *written = length;
		return GF_OK;
	}
	if (sock->cap_info) {
		if (sock->cap_info->patch_offset) {
			((u8*)buffer)[sock->cap_info->patch_offset-1] = sock->cap_info->patch_val;
			sock->cap_info->patch_offset = 0;
		}
		if (sock->cap_info->nf->read_socks==NULL)
			return gf_netcap_send(sock, buffer, length, written);

		if (sock->cap_info->nf->read_socks)
			return GF_OK;
	}
#endif

	//the socket must be bound or connected
	if (!sock || !sock->socket)
		return GF_BAD_PARAM;

	if (! (sock->flags & GF_SOCK_NON_BLOCKING)) {
		//check write
		GF_Err e = poll_select(sock, GF_SK_SELECT_WRITE, sock->usec_wait, GF_FALSE);
		if (e) return e;
	}

	//direct writing
	count = 0;
	while (count < length) {
		if (sock->flags & GF_SOCK_HAS_PEER) {
			res = (s32) sendto(sock->socket, (char *) buffer+count,  length - count, 0, (struct sockaddr *) &sock->dest_addr, sock->dest_addr_len);
		} else {
			int sflags = 0;
#ifdef MSG_NOSIGNAL
			sflags = MSG_NOSIGNAL;
#endif
			res = (s32) send(sock->socket, (char *) buffer+count, length - count, sflags);
		}
		if (res == SOCKET_ERROR) {
			switch (res = LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_NETWORK_EMPTY;
#ifndef __SYMBIAN32__
			case ENOTCONN:
			case ECONNRESET:
			case EPIPE:
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[socket] send failure: %s\n", gf_errno_str(LASTSOCKERROR)));
				return GF_IP_CONNECTION_CLOSED;
#endif

#ifndef __DARWIN__
			case EPROTOTYPE:
				return GF_IP_NETWORK_EMPTY;
#endif
			case ENOBUFS:
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[socket] send failure: %s\n", gf_errno_str(LASTSOCKERROR)));
				return GF_BUFFER_TOO_SMALL;
			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] send failure: %s\n", gf_errno_str(LASTSOCKERROR)));
				return GF_IP_NETWORK_FAILURE;
			}
		}
		count += res;
		if (written) *written += res;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_send(GF_Socket *sock, const u8 *buffer, u32 length)
{
	return gf_sk_send_ex(sock, buffer, length, NULL);

}

GF_Err gf_sk_select(GF_Socket *sock, GF_SockSelectMode mode)
{
	//the socket must be bound or connected
	if (!sock || !sock->socket)
		return GF_BAD_PARAM;

	//check write and read
	return poll_select(sock, mode, sock->usec_wait, GF_FALSE);
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
	//mapped address
	if (!strnicmp(multi_IPAdd, "::ffff:", 7)) multi_IPAdd+=7;
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

static GF_Err sk_join_ipv4(GF_Socket *sock, struct ip_mreq *M_req, u32 TTL, const char **src_ip_inc, u32 nb_src_ip_inc, const char **src_ip_exc, u32 nb_src_ip_exc)
{
	s32 ret;
	u32 flag;

	if (nb_src_ip_inc) {
#ifdef IP_ADD_SOURCE_MEMBERSHIP
		u32 i;
		struct ip_mreq_source M_req_src;
		M_req_src.imr_multiaddr = M_req->imr_multiaddr;
		M_req_src.imr_interface = M_req->imr_interface;

		for (i=0; i<nb_src_ip_inc; i++) {
			if (gf_net_is_ipv6(src_ip_inc[i])) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] IPv6 SSM source on IPv4 Mcast address, ignoring\n", src_ip_inc[i]));
				continue;
			}
			M_req_src.imr_sourceaddr.s_addr = inet_addr(src_ip_inc[i]);
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *) &M_req_src, sizeof(M_req_src) );
			if (ret == SOCKET_ERROR) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] cannot add source membership for %s: %s\n", src_ip_inc[i], gf_errno_str(LASTSOCKERROR)));
				return GF_IP_CONNECTION_FAILURE;
			}
		}
#else
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] SSM IPv4 not supported\n"));
		return GF_NOT_SUPPORTED;
#endif
	} else {
		ret = setsockopt(sock->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) M_req, sizeof(*M_req));
		if (ret == SOCKET_ERROR) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] cannot join multicast: %s\n", gf_errno_str(LASTSOCKERROR)));
			return GF_IP_CONNECTION_FAILURE;
		}
	}
	/*set the Time To Live*/
	if (TTL) {
		ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&TTL, sizeof(TTL));
		if (ret == SOCKET_ERROR) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Socket] Cannot set TTL: %s\n", gf_errno_str(LASTSOCKERROR) ));
			return GF_IP_CONNECTION_FAILURE;
		}
	}

	/*enable loopback*/
	flag = 1;
	ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &flag, sizeof(flag));
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Socket] Cannot enable multicast loop: %s - ignoring\n", gf_errno_str(LASTSOCKERROR) ));
	}

#ifdef IP_BLOCK_SOURCE
	if (nb_src_ip_exc) {
		u32 i, j;
		struct ip_mreq_source M_req_exc;
		M_req_exc.imr_multiaddr = M_req->imr_multiaddr;
		M_req_exc.imr_interface = M_req->imr_interface;
		for (i=0; i<nb_src_ip_exc; i++) {
			Bool match=GF_FALSE;
			if (gf_net_is_ipv6(src_ip_exc[i])) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] IPv6 SSM source on IPv4 Mcast address, ignoring\n", src_ip_exc[i]));
				continue;
			}
			for (j=0; j<nb_src_ip_inc; j++) {
				if (!strcmp(src_ip_inc[j], src_ip_exc[i])) match=GF_TRUE;
			}
			if (match) continue;

			M_req_exc.imr_sourceaddr.s_addr = inet_addr(src_ip_exc[i]);
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_BLOCK_SOURCE, (char *) &M_req_exc, sizeof(M_req_exc) );
			if (ret == SOCKET_ERROR) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] cannot block source membership for %s: %s\n", src_ip_exc[i], gf_errno_str(LASTSOCKERROR)));
				return GF_IP_CONNECTION_FAILURE;
			}
		}
	}
#else
	GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[core] SSM IPv4 not supported\n"));
#endif
	return GF_OK;
}

static u32 inet_addr_from_name(const char *local_interface)
{
	if (!local_interface) return htonl(INADDR_ANY);

	if (strchr(local_interface, '.'))
		return inet_addr(local_interface);

	Bool name_match = GF_FALSE;
	u32 ret = 0;

#if defined(GPAC_WIN_HAS_ADAPTER_INFO)
	IP_ADAPTER_ADDRESSES *AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES));
	IP_ADAPTER_ADDRESSES *ifa = NULL;
	ULONG ulOutBufLen = sizeof(AdapterInfo);
	if (AdapterInfo && (GetAdaptersAddresses(AF_INET, 0, NULL, AdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)) {
		free(AdapterInfo);
		AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES)*ulOutBufLen);
	}
	if (AdapterInfo)
		GetAdaptersAddresses(AF_INET, 0, NULL, AdapterInfo, &ulOutBufLen);

	ifa = AdapterInfo;
	while (ifa) {
		Bool match = GF_FALSE;
		if (!strcmp(ifa->AdapterName, local_interface)) match = GF_FALSE;
		char *name = gf_wcs_to_utf8(ifa->FriendlyName);
		if (name) {
			if (!strcmp(name, local_interface)) match = GF_TRUE;
			gf_free(name);
		}
		if (match) name_match = GF_TRUE;

		if (!match || !ifa->FirstUnicastAddress || !ifa->FirstUnicastAddress->Address.lpSockaddr) {
			ifa = ifa->Next;
			continue;
		}
		ret = ((struct sockaddr_in*)ifa->FirstUnicastAddress->Address.lpSockaddr)->sin_addr.s_addr;
		break;
	}
	if (AdapterInfo)
		free(AdapterInfo);
#endif

#if defined(GPAC_HAS_IFADDRS)
	struct ifaddrs *ifap=NULL, *res;
	getifaddrs(&ifap);
	res = ifap;
	while (res) {
		if (res->ifa_name && !strcmp(res->ifa_name, local_interface)) {
			name_match = GF_TRUE;
			if (res->ifa_addr && (res->ifa_addr->sa_family==AF_INET)) {
				ret = ((struct sockaddr_in*)res->ifa_addr)->sin_addr.s_addr;
				break;
			}
		}
		res = res->ifa_next;
	}
	freeifaddrs(ifap);
#endif
	if (!name_match) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] No such interface %s\n", local_interface));
		return 0xFFFFFFFF;
	}
	if (!ret) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] Interface %s found but has no IPv4 addressing, using INADRR_ANY\n", local_interface));
		return htonl(INADDR_ANY);
	}
	return ret;
}

GF_EXPORT
Bool gf_net_enum_interfaces(gf_net_ifce_enum do_enum, void *enum_cbk)
{
#if defined(GPAC_WIN_HAS_ADAPTER_INFO)
	IP_ADAPTER_ADDRESSES *AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES));
	IP_ADAPTER_ADDRESSES *ifa = NULL;
	ULONG ulOutBufLen = sizeof(AdapterInfo);
	if (AdapterInfo && (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)) {
		free(AdapterInfo);
		AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES)*ulOutBufLen);
	}
	if (AdapterInfo)
		GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterInfo, &ulOutBufLen);

	ifa = AdapterInfo;
	while (ifa) {
		if (ifa->FirstUnicastAddress) {
			char szIP[100];
			szIP[0] = 0;
			u32 flags = 0;
			if (ifa->OperStatus == 1) flags |= GF_NETIF_ACTIVE;
			if (ifa->Flags & IP_ADAPTER_NO_MULTICAST) flags |= GF_NETIF_NO_MCAST;
			if (ifa->Flags & IP_ADAPTER_RECEIVE_ONLY) flags |= GF_NETIF_RECV_ONLY;
			if (ifa->IfType == IF_TYPE_SOFTWARE_LOOPBACK) flags |= GF_NETIF_LOOPBACK;

			Bool abort=GF_FALSE;
			IP_ADAPTER_UNICAST_ADDRESS_LH *ipadd = ifa->FirstUnicastAddress;
			while (ipadd) {
				u32 sa_fam = ipadd->Address.lpSockaddr ? ipadd->Address.lpSockaddr->sa_family : AF_UNSPEC;
				if (ipadd->Flags & IP_ADAPTER_ADDRESS_TRANSIENT) sa_fam = AF_UNSPEC;
				if ((sa_fam == AF_INET) || (sa_fam == AF_INET6)) {
					my_inet_ntop(sa_fam, ipadd->Address.lpSockaddr, szIP, 100);
					char *name = gf_wcs_to_utf8(ifa->FriendlyName);
					Bool res = do_enum(enum_cbk, name ? name : ifa->AdapterName, szIP[0] ? szIP : NULL, flags | ((sa_fam==AF_INET6) ? GF_NETIF_IPV6 : 0));
					if (name) gf_free(name);
					if (res) break;
				}
				ipadd = ipadd->Next;
			}
			if (abort) break;
		}
		ifa = ifa->Next;
	}
	if (AdapterInfo)
		free(AdapterInfo);
	return GF_TRUE;
#endif

#if defined(GPAC_HAS_IFADDRS)
	struct ifaddrs *ifap=NULL, *res;
	getifaddrs(&ifap);
	res = ifap;
	while (res) {
		if (res->ifa_addr) {
			char szIP[100];
			szIP[0] = 0;
			u32 flags=0;
			if (res->ifa_flags & IFF_UP) flags |= GF_NETIF_ACTIVE;
			if (res->ifa_flags & IFF_LOOPBACK) flags |= GF_NETIF_LOOPBACK;
			if (!(res->ifa_flags & IFF_MULTICAST)) flags |= GF_NETIF_NO_MCAST;

			if (res->ifa_addr->sa_family==AF_INET) {
				inet_ntop(AF_INET, &( ((struct sockaddr_in *)res->ifa_addr)->sin_addr), szIP, 100);
				if (do_enum(enum_cbk, res->ifa_name, szIP[0] ? szIP : NULL, flags))
					break;
			}
			if (res->ifa_addr->sa_family==AF_INET6) {
				inet_ntop(AF_INET6, &( ((struct sockaddr_in6 *)res->ifa_addr)->sin6_addr), szIP, 100);
				if (do_enum(enum_cbk, res->ifa_name, szIP[0] ? szIP : NULL, flags|GF_NETIF_IPV6))
					break;
			}
		}
		res = res->ifa_next;
	}
	freeifaddrs(ifap);
	return GF_TRUE;
#endif
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_sk_setup_multicast_ex(GF_Socket *sock, const char *multi_IPAdd, u16 MultiPortNumber, u32 TTL, Bool NoBind, char *ifce_ip_or_name,
	const char **src_ip_inc, u32 nb_src_ip_inc, const char **src_ip_exc, u32 nb_src_ip_exc)
{
	s32 ret;
	struct ip_mreq M_req;
	u32 optval;
#ifdef GPAC_HAS_IPV6
	struct sockaddr *addr;
	Bool mcast_ipv6 = GF_FALSE;
	Bool ifce_ipv6 = GF_FALSE;
	u32 type;
#endif
	GF_Err e;
	u32 local_add_id;

	if (!sock || sock->socket) return GF_BAD_PARAM;

	if (TTL > 255) TTL = 255;

	/*check the address*/
	if (!gf_sk_is_multicast_address(multi_IPAdd)) return GF_BAD_PARAM;


#ifdef GPAC_HAS_IPV6
	mcast_ipv6 = gf_net_is_ipv6(multi_IPAdd);
	ifce_ipv6 = gf_net_is_ipv6(ifce_ip_or_name);
	if (ifce_ip_or_name && (ifce_ip_or_name[0]=='+')) {
		ifce_ip_or_name += 1;
		ifce_ipv6 = GF_TRUE;
	}
	if (ifce_ip_or_name && !ifce_ip_or_name[0])
		ifce_ip_or_name=NULL;

	type = (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM;

	if (mcast_ipv6 || ifce_ipv6) {
		Bool is_mapped_v4=GF_FALSE;
		struct addrinfo *res, *aip;
		struct ipv6_mreq M_reqV6;
		memset(&M_reqV6, 0, sizeof(struct ipv6_mreq));

		//get interface index - force IPV6 when getting address and store result
		if (ifce_ip_or_name) {
#if defined(GPAC_WIN_HAS_ADAPTER_INFO)
			IP_ADAPTER_ADDRESSES *AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES));
			ULONG ulOutBufLen = sizeof(AdapterInfo);
			if (AdapterInfo && (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)) {
				free(AdapterInfo);
				AdapterInfo = malloc(sizeof(IP_ADAPTER_ADDRESSES)*ulOutBufLen);
			}
			if (AdapterInfo)
				GetAdaptersAddresses(AF_UNSPEC, 0, NULL, AdapterInfo, &ulOutBufLen);

			IP_ADAPTER_ADDRESSES *ifa = AdapterInfo;
			while (ifa) {
				//match by adapter name
				if (!strcmp(ifa->AdapterName, ifce_ip_or_name)) {
					M_reqV6.ipv6mr_interface = ifa->Ipv6IfIndex;
					break;
				}
				char *name = gf_wcs_to_utf8(ifa->FriendlyName);
				if (name && !strcmp(name, ifce_ip_or_name)) {
					M_reqV6.ipv6mr_interface = ifa->Ipv6IfIndex;
					gf_free(name);
					break;
				}
				if (name) gf_free(name);
				name = gf_wcs_to_utf8(ifa->Description);
				if (!strcmp(name, ifce_ip_or_name)) {
					M_reqV6.ipv6mr_interface = ifa->Ipv6IfIndex;
					gf_free(name);
					break;
				}
				if (name) gf_free(name);

				//match by IP
				IP_ADAPTER_UNICAST_ADDRESS_LH *ipadd = ifa->FirstUnicastAddress;
				while (ipadd) {
					u32 sa_fam = ipadd->Address.lpSockaddr ? ipadd->Address.lpSockaddr->sa_family : AF_UNSPEC;
					if ((sa_fam == AF_INET) || (sa_fam == AF_INET6)) {
						char szIP[100];
						my_inet_ntop(sa_fam, ipadd->Address.lpSockaddr, szIP, 100);
						if (!strcmp(szIP, ifce_ip_or_name)) {
							M_reqV6.ipv6mr_interface = ifa->Ipv6IfIndex;
							break;
						}
					}
					ipadd = ipadd->Next;
				}

				ifa = ifa->Next;
			}
			if (AdapterInfo) free(AdapterInfo);
#endif

#if defined(GPAC_HAS_IFADDRS)
			//get sockaddr if this is an IP address and compare with enumerated interface
			aip = NULL;
			if (ifce_ip_or_name && strpbrk(ifce_ip_or_name, ".:")) {
				aip = gf_sk_get_ipv6_addr(ifce_ip_or_name, MultiPortNumber, ifce_ipv6 ? AF_INET6 : AF_UNSPEC, AI_PASSIVE, type);
			}
			struct ifaddrs *ifap=NULL, *ifa;
			getifaddrs(&ifap);
			ifa = ifap;
			while (ifa) {
				//match by ifce name
				if (ifa->ifa_addr && !strcmp(ifa->ifa_name, ifce_ip_or_name)
					&& ((ifa->ifa_addr->sa_family == AF_INET) || (ifa->ifa_addr->sa_family == AF_INET6))
				) {
					M_reqV6.ipv6mr_interface = if_nametoindex(ifa->ifa_name);
					break;
				}
				//match by ifce IP
				if (aip && ifa->ifa_addr) {
					struct sockaddr_in6 *in6_a = (struct sockaddr_in6 *)aip->ai_addr;
					struct sockaddr_in6 *in6_b = (struct sockaddr_in6 *)ifa->ifa_addr;
					if ((in6_a->sin6_family == in6_b->sin6_family)
						&& !memcmp(&in6_a->sin6_addr, &in6_b->sin6_addr, sizeof(struct in6_addr))
					) {

						M_reqV6.ipv6mr_interface = if_nametoindex(ifa->ifa_name);
						break;
					}
				}
				ifa = ifa->ifa_next;
			}
			if (ifap)
				freeifaddrs(ifap);
			if (aip)
				freeaddrinfo(aip);
#endif

			if (!M_reqV6.ipv6mr_interface) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] Failed to query interface index for %s\n", ifce_ip_or_name ));
				return GF_IP_CONNECTION_FAILURE;
			}
		}

		//do not specify local_interface_ip address
		res = gf_sk_get_ipv6_addr(NULL, MultiPortNumber, ifce_ipv6 ? AF_INET6 : AF_UNSPEC, AI_PASSIVE, type);
		if (!res) return GF_IP_CONNECTION_FAILURE;

		/*for all interfaces*/
		for (aip=res; aip!=NULL; aip=aip->ai_next) {
			if (type != (u32) aip->ai_socktype) continue;
			//we have a v4 multicast address
			if (!gf_net_is_ipv6(multi_IPAdd)) {
				//if not v4 and next is v4, use next
				if ((aip->ai_family!=PF_INET) && aip->ai_next && (aip->ai_next->ai_family==PF_INET)) continue;
			} else {
				//we want v6, if v4 and next is v6, use next
				if ((aip->ai_family==PF_INET) && aip->ai_next && (aip->ai_next->ai_family==PF_INET6)) continue;
			}
			sock->socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
			if (sock->socket == INVALID_SOCKET) {
				sock->socket = NULL_SOCKET;
				continue;
			}

			/*enable address reuse*/
			optval = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));
#ifdef SO_REUSEPORT
			optval = 1;
			setsockopt(sock->socket, SOL_SOCKET, SO_REUSEPORT, SSO_CAST &optval, sizeof(optval));
#endif

			if (!mcast_ipv6 && (aip->ai_family == PF_INET6)) {
				optval = 0;
				ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&optval, sizeof(optval));
				if (ret == SOCKET_ERROR) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Socket] Cannot disable ipv6only: %s\n", gf_errno_str(LASTSOCKERROR)));
				}
			}

			if (!NoBind) {
				ret = bind(sock->socket, (struct sockaddr *) aip->ai_addr, (int) aip->ai_addrlen);
				if (ret == SOCKET_ERROR) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] cannot bind socket: %s\n", gf_errno_str(LASTSOCKERROR)));
					return GF_IP_CONNECTION_FAILURE;
				}
			}
			if (aip->ai_family==PF_INET6) sock->flags |= GF_SOCK_IS_IPV6;
			else sock->flags &= ~GF_SOCK_IS_IPV6;
			break;
		}
		freeaddrinfo(res);

		if (!sock->socket) return GF_IP_CONNECTION_FAILURE;

		/*TODO: copy over other properties (recption buffer size & co)*/
		if (sock->flags & GF_SOCK_NON_BLOCKING)
			gf_sk_set_block_mode(sock, GF_TRUE);

		res = gf_sk_get_ipv6_addr(multi_IPAdd, MultiPortNumber, (sock->flags & GF_SOCK_IS_IPV6) ? AF_INET6 : AF_UNSPEC, 0, SOCK_DGRAM);
		if (!res) {
			if (sock->flags & GF_SOCK_IS_IPV6) {
				char szIP[100];
				strcpy(szIP, "::ffff:");
				strcat(szIP, multi_IPAdd);
				is_mapped_v4 = GF_TRUE;
				res = gf_sk_get_ipv6_addr(szIP, MultiPortNumber, AF_INET6, 0, SOCK_DGRAM);
			}
			if (!res)
				return GF_IP_CONNECTION_FAILURE;
		}
		//copy for sendto
		memcpy(&sock->dest_addr, res->ai_addr, res->ai_addrlen);
		sock->dest_addr_len = (u32) res->ai_addrlen;
		freeaddrinfo(res);

		addr = (struct sockaddr *)&sock->dest_addr;
		//ipv4
		if (is_mapped_v4) {
			local_add_id = inet_addr_from_name(ifce_ip_or_name);
			if (local_add_id==0xFFFFFFFF) return GF_IP_CONNECTION_FAILURE;

			M_req.imr_multiaddr.s_addr = inet_addr(multi_IPAdd);
			M_req.imr_interface.s_addr = local_add_id;

			e = sk_join_ipv4(sock, &M_req, TTL, src_ip_inc, nb_src_ip_inc, src_ip_exc, nb_src_ip_exc);
			if (e) return e;
			//keep sock->dest_addr untouched so that we still have a ipv6 sockaddr
			sock->flags |= GF_SOCK_IS_MULTICAST | GF_SOCK_HAS_PEER;
			return GF_OK;
		}
		else if (addr->sa_family != AF_INET6) {
			return GF_IP_CONNECTION_FAILURE;
		}

		memcpy(&M_reqV6.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)addr)->sin6_addr), sizeof(struct in6_addr));

		/*set send IF index*/
		optval = M_reqV6.ipv6mr_interface;
		ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *)&optval, sizeof(optval));
		if (ret == SOCKET_ERROR) {
		}

		if (nb_src_ip_inc) {
#ifdef MCAST_JOIN_SOURCE_GROUP
			u32 i;
			struct group_source_req M_reqV6_src;
			memset(&M_reqV6_src, 0, sizeof(struct group_source_req));
			M_reqV6_src.gsr_interface = 0;
			M_reqV6_src.gsr_group = sock->dest_addr;

			for (i=0; i<nb_src_ip_inc; i++) {
				res = gf_sk_get_ipv6_addr(src_ip_inc[i], MultiPortNumber, AF_INET6, 0, SOCK_DGRAM);
				if (!res) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] Failed to get address info for SSM source %s, ignoring\n", src_ip_inc[i]));
					continue;
				}
				memcpy(&M_reqV6_src.gsr_source, res->ai_addr, res->ai_addrlen);
				freeaddrinfo(res);
				ret = setsockopt(sock->socket, IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP, (char *) &M_reqV6_src, sizeof(M_reqV6_src) );
				if (ret == SOCKET_ERROR) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] cannot join source group %s: %s\n", src_ip_inc[i], gf_errno_str(LASTSOCKERROR)));
					return GF_IP_CONNECTION_FAILURE;
				}
			}
#else
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] SSM IPv6 not supported\n"));
			return GF_NOT_SUPPORTED;
#endif

		} else {
			ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char *) &M_reqV6, sizeof(M_reqV6));
			if (ret == SOCKET_ERROR) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] Failed to join multicast: %s\n", gf_errno_str(LASTSOCKERROR) ));
				return GF_IP_CONNECTION_FAILURE;
			}
		}

		/*set TTL*/
		ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &TTL, sizeof(TTL));
		if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		/*enable loopback*/
		optval = 1;
		ret = setsockopt(sock->socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &optval, sizeof(optval));
		if (ret == SOCKET_ERROR) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[Socket] Cannot enable multicast loop: %s\n", gf_errno_str(LASTSOCKERROR) ));
		}

		if (nb_src_ip_exc) {
#ifdef MCAST_BLOCK_SOURCE
			u32 i, j;
			struct group_source_req M_reqV6_src;
			M_reqV6_src.gsr_interface = 0;
			M_reqV6_src.gsr_group = sock->dest_addr;
			for (i=0; i<nb_src_ip_exc; i++) {
				Bool match=GF_FALSE;
				for (j=0; j<nb_src_ip_inc; j++) {
					if (!strcmp(src_ip_inc[j], src_ip_exc[i])) match=GF_TRUE;
				}
				if (match) continue;

				res = gf_sk_get_ipv6_addr(src_ip_exc[i], MultiPortNumber, AF_INET6, 0, SOCK_DGRAM);
				if (!res) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] Failed to get address info for SSM excluded source %s, ignoring\n", src_ip_exc[i]));
					continue;
				}
				memcpy(&M_reqV6_src.gsr_source, res->ai_addr, res->ai_addrlen);
				freeaddrinfo(res);

				ret = setsockopt(sock->socket, IPPROTO_IPV6, MCAST_BLOCK_SOURCE, (char *) &M_reqV6_src, sizeof(M_reqV6_src) );
				if (ret == SOCKET_ERROR) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[core] cannot block source membership for %s: %s\n", src_ip_exc[i], gf_errno_str(LASTSOCKERROR)));
					return GF_IP_CONNECTION_FAILURE;
				}
			}
#else
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[core] SSM IPv6 not supported\n"));
#endif
		}
		sock->flags |= GF_SOCK_IS_MULTICAST | GF_SOCK_HAS_PEER;
		return GF_OK;
	}
#endif

#ifndef GPAC_DISABLE_NETCAP
	if (sock->cap_info && sock->cap_info->nf->cap_mode) {
		sock->cap_info->host_port = MultiPortNumber;
		sock->cap_info->host_addr_v4 = inet_addr(multi_IPAdd);
		sock->flags &= ~GF_SOCK_IS_IPV6;
		sock->flags |= GF_SOCK_IS_MULTICAST;
		return GF_OK;
	}
#endif


	//IPv4 setup
	sock->socket = socket(AF_INET, (sock->flags & GF_SOCK_IS_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock->flags & GF_SOCK_NON_BLOCKING)
		gf_sk_set_block_mode(sock, GF_TRUE);
	sock->flags &= ~GF_SOCK_IS_IPV6;

	/*enable address reuse*/
	optval = 1;
	ret = setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, SSO_CAST &optval, sizeof(optval));
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[core] Failed to set SO_REUSEADDR: %s\n", gf_errno_str(LASTSOCKERROR) ));
	}
#ifdef SO_REUSEPORT
	optval = 1;
	ret = setsockopt(sock->socket, SOL_SOCKET, SO_REUSEPORT, SSO_CAST &optval, sizeof(optval));
	if (ret == SOCKET_ERROR) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[core] Failed to set SO_REUSEPORT: %s\n", gf_errno_str(LASTSOCKERROR) ));
	}
#endif

	local_add_id = inet_addr_from_name(ifce_ip_or_name);
	if (local_add_id==0xFFFFFFFF) return GF_IP_CONNECTION_FAILURE;

	if (!NoBind) {
		struct sockaddr_in local_address;

		memset(&local_address, 0, sizeof(struct sockaddr_in ));
		local_address.sin_family = AF_INET;
		local_address.sin_addr.s_addr = htonl(INADDR_ANY);
		local_address.sin_port = htons( MultiPortNumber);

		ret = bind(sock->socket, (struct sockaddr *) &local_address, sizeof(local_address));
		if (ret == SOCKET_ERROR) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[core] Failed to bind socket: %s\n", gf_errno_str(LASTSOCKERROR) ));
			return GF_IP_CONNECTION_FAILURE;
		}
		/*setup local interface*/
		if (ifce_ip_or_name) {
			ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_IF, (void *) &local_add_id, sizeof(local_add_id));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		}
	}

	/*now join the multicast*/
	M_req.imr_multiaddr.s_addr = inet_addr(multi_IPAdd);
	M_req.imr_interface.s_addr = local_add_id;

	e = sk_join_ipv4(sock, &M_req, TTL, src_ip_inc, nb_src_ip_inc, src_ip_exc, nb_src_ip_exc);
	if (e) return e;

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

GF_EXPORT
GF_Err gf_sk_setup_multicast(GF_Socket *sock, const char *multi_IPAdd, u16 MultiPortNumber, u32 TTL, Bool NoBind, char *local_interface_ip)
{
	return gf_sk_setup_multicast_ex(sock, multi_IPAdd, MultiPortNumber, TTL, NoBind, local_interface_ip, NULL, 0, NULL, 0);

}

#include <gpac/list.h>
struct __tag_sock_group
{
	GF_List *sockets;
	fd_set rgroup, wgroup;

#ifdef GPAC_HAS_POLL
	u32 last_mask;
	u32 nb_fds, alloc_fds;
	GF_POLLFD *fds;
#endif

#ifndef GPAC_DISABLE_NETCAP
	u32 nb_nfs;
	u32 nb_socks;
#endif
};

GF_SockGroup *gf_sk_group_new()
{
	GF_SockGroup *tmp;
	GF_SAFEALLOC(tmp, GF_SockGroup);
	if (!tmp) return NULL;
	FD_ZERO(&tmp->rgroup);
	FD_ZERO(&tmp->wgroup);

#ifdef GPAC_HAS_POLL
	tmp->last_mask = POLLIN;
#endif
	return tmp;
}

void gf_sk_group_del(GF_SockGroup *sg)
{
	gf_list_del(sg->sockets);
#ifdef GPAC_HAS_POLL
	if (sg->fds) gf_free(sg->fds);
#endif
	gf_free(sg);
}

void gf_sk_group_register(GF_SockGroup *sg, GF_Socket *sk)
{
	if (!sg || !sk) return;
	if (!sg->sockets) sg->sockets = gf_list_new();
	if (gf_list_find(sg->sockets, sk)>=0) return;
	gf_list_add(sg->sockets, sk);

#ifndef GPAC_DISABLE_NETCAP
	if (sk->cap_info) {
		sg->nb_nfs++;
		sg->nb_socks = gf_list_count(sg->sockets);
		return;
	}
#endif

#ifdef GPAC_HAS_POLL
	if (!sg->fds && !gpac_use_poll)
		return;

	if (sg->nb_fds + 1 > sg->alloc_fds) {
		sg->fds = gf_realloc(sg->fds, (sg->nb_fds+1) * sizeof(GF_POLLFD));
		sg->alloc_fds = sg->nb_fds+1;
	}
	sg->fds[sg->nb_fds].fd = sk->socket;
	sg->fds[sg->nb_fds].events = sg->last_mask;
	sg->fds[sg->nb_fds].revents = 0;
	sk->poll_idx = sg->nb_fds+1;
	gf_assert(sg->fds[sg->nb_fds].fd != 0);
	sg->nb_fds++;
#endif
}

void gf_sk_group_unregister(GF_SockGroup *sg, GF_Socket *sk)
{
	if (!sg || !sk) return;
	s32 pidx = gf_list_del_item(sg->sockets, sk);

#ifndef GPAC_DISABLE_NETCAP
	if (sk->cap_info && sk->cap_info->nf->read_socks) {
		if (sg->nb_nfs)
			sg->nb_nfs--;
		sg->nb_socks = gf_list_count(sg->sockets);

		if (sk == sk->cap_info->nf->read_sock_selected) {
			gf_bs_skip_bytes(sk->cap_info->nf->cap_bs, sk->cap_info->nf->pck_len);
			sk->cap_info->nf->dst_port = 0;
			sk->cap_info->nf->read_sock_selected=NULL;
			//do not load a new packet here as this could go into infinite loop when netcap-loop=-1 ...
		}
		return;
	}
#endif

	if (!gf_list_count(sg->sockets)) {
		gf_list_del(sg->sockets);
		sg->sockets = NULL;
#ifdef GPAC_HAS_POLL
		if (sg->fds) {
			gf_free(sg->fds);
			sg->fds = NULL;
			sg->nb_fds = sg->alloc_fds = 0;
		}
#endif
		return;
	}
	if (pidx<0) return;

#ifdef GPAC_HAS_POLL
	sk->poll_idx=0;
	if (!sg->fds) return;
	memmove(&sg->fds[pidx], &sg->fds[pidx+1], sizeof(GF_POLLFD) * (sg->nb_fds-pidx-1));
	//reassign poll file descriptors and poll index
	sg->nb_fds--;
	while (1) {
		GF_Socket *asock = gf_list_get(sg->sockets, pidx);
		if (!asock) break;
		sg->fds[pidx].fd = asock->socket;
		asock->poll_idx = pidx+1;
		pidx++;
	}
#endif
}

GF_Err gf_sk_group_select(GF_SockGroup *sg, u32 usec_wait, GF_SockSelectMode mode)
{
	s32 ready;
	u32 i;
	struct timeval timeout;
	GF_Socket *sock;

	if (!sg->sockets)
		return GF_IP_NETWORK_EMPTY;

#ifndef GPAC_DISABLE_NETCAP
	if (sg->nb_nfs) {
		u32 nb_closed=0;
		u32 nb_write=0;
		u32 nb_nofs=0;
		u32 nb_tcp=0;
		u64 now = gf_sys_clock_high_res();
		for (i=0; i<sg->nb_socks; i++) {
			GF_Socket *s = gf_list_get(sg->sockets, i);
			if (!s->cap_info) {
				nb_nofs++;
				continue;
			}
			if (!s->cap_info->nf->read_socks) {
				nb_write++;
				continue;
			}

			gf_netcap_load_pck(s->cap_info->nf, now);
			if (!s->cap_info->nf->read_sock_selected) {
				if (s->cap_info->nf->is_eos) {
					if (s->flags & GF_SOCK_IS_TCP) nb_tcp++;
					nb_closed++;
				}
				continue;
			}

			if (s->cap_info->nf->rt && (s->cap_info->nf->pck_time > now))
				continue; // GF_IP_NETWORK_EMPTY;

			if (s == s->cap_info->nf->read_sock_selected)
				return GF_OK;
		}
		if (nb_closed==sg->nb_socks) return nb_tcp ? GF_IP_CONNECTION_CLOSED : GF_EOS;
		if (nb_write) return GF_OK;
		if (!nb_nofs) return GF_IP_NETWORK_EMPTY;
		//fallthrough
	}
#endif

#ifdef GPAC_HAS_POLL
	if (sg->fds) {
		u32 mask = 0;
		if (mode == GF_SK_SELECT_BOTH) mask = POLLIN | POLLOUT;
		else if (mode == GF_SK_SELECT_READ) mask = POLLIN;
		else mask = POLLOUT;
		if (sg->last_mask != mask) {
			sg->last_mask = mask;
			for (i=0; i<sg->nb_fds; i++) {
				sg->fds[i].events = mask;
				gf_assert(sg->fds[i].fd != 0);
			}
		}
		int res = poll(sg->fds, sg->nb_fds, usec_wait/1000);
		if (res<0) {
			switch (LASTSOCKERROR) {
			case EAGAIN:
				return GF_IP_NETWORK_EMPTY;
			default:
				GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot poll: %s\n", gf_errno_str(LASTSOCKERROR) ));
				return GF_IP_NETWORK_FAILURE;
			}
		}

		if (!res)
			return GF_IP_NETWORK_EMPTY;
		return GF_OK;
	}
#endif

	i=0;
	u32 max_fd=0;
	fd_set *rgroup=NULL, *wgroup=NULL;

	FD_ZERO(&sg->rgroup);
	FD_ZERO(&sg->wgroup);

	switch (mode) {
	case GF_SK_SELECT_BOTH:
		rgroup = &sg->rgroup;
		wgroup = &sg->wgroup;
		break;
	case GF_SK_SELECT_READ:
		rgroup = &sg->rgroup;
		break;
	case GF_SK_SELECT_WRITE:
		wgroup = &sg->wgroup;
		break;
	}
	while ((sock = gf_list_enum(sg->sockets, &i))) {
		if (rgroup)
			FD_SET(sock->socket, rgroup);

		if (wgroup)
			FD_SET(sock->socket, wgroup);

		if (max_fd < (u32) sock->socket) max_fd = (u32) sock->socket;
	}
	timeout.tv_sec = 0;
	timeout.tv_usec = usec_wait;
	ready = select((int) max_fd+1, rgroup, wgroup, NULL, &timeout);

	if (ready == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EBADF:
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot select, BAD descriptor\n"));
			return GF_IP_CONNECTION_CLOSED;
		case EAGAIN:
			return GF_IP_NETWORK_EMPTY;
		case EINTR:
			/* Interrupted system call*/
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] network is lost\n"));
			return GF_IP_NETWORK_EMPTY;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] cannot select: %s\n", gf_errno_str(LASTSOCKERROR) ));
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!ready) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[socket] nothing to be read - ready %d\n", ready));
		return GF_IP_NETWORK_EMPTY;
	}
	return GF_OK;
}

Bool gf_sk_group_sock_is_set(GF_SockGroup *sg, GF_Socket *sk, GF_SockSelectMode mode)
{
	if (!sg || !sk) return GF_FALSE;

#ifndef GPAC_DISABLE_NETCAP
	if (sk->cap_info) {
		//write, always true
		if (!sk->cap_info->nf->read_socks) return GF_TRUE;
		//read
		if (sk->cap_info->nf->read_sock_selected != sk) return GF_FALSE;
		if (sk->cap_info->nf->rt && (sk->cap_info->nf->pck_time > gf_sys_clock_high_res()))
			return GF_FALSE;

		return GF_TRUE;
	}
#endif

#ifdef GPAC_HAS_POLL
	if (sg->fds && sk->poll_idx) {
		GF_POLLFD *pfd = &sg->fds[sk->poll_idx-1];
		//disconnected, consider ready to read/write
		if (pfd->revents & POLLHUP)
			return GF_TRUE;

		if ((mode!=GF_SK_SELECT_WRITE) && (pfd->revents & POLLIN))
			return GF_TRUE;

		if ((mode!=GF_SK_SELECT_READ) && (pfd->revents & POLLOUT))
			return GF_TRUE;

//		if (gf_sk_probe(sk) == GF_IP_CONNECTION_CLOSED)
//			return GF_TRUE;
		return GF_FALSE;
	}
#endif

	if ((mode!=GF_SK_SELECT_WRITE) && FD_ISSET(sk->socket, &sg->rgroup))
		return GF_TRUE;
	if ((mode!=GF_SK_SELECT_READ) && FD_ISSET(sk->socket, &sg->wgroup))
		return GF_TRUE;

	return GF_FALSE;
}

//fetch nb bytes on a socket and fill the buffer from startFrom
//length is the allocated size of the receiving buffer
//BytesRead is the number of bytes read from the network
GF_Err gf_sk_receive_internal(GF_Socket *sock, char *buffer, u32 length, u32 *BytesRead, Bool do_select)
{
	s32 res;

	if (BytesRead) *BytesRead = 0;
	if (!sock) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_NETCAP
	if (sock->cap_info) {
		if (!sock->cap_info->nf->read_socks) return GF_IP_NETWORK_EMPTY;

		GF_NetcapFilter *nf = sock->cap_info->nf;
		if (do_select && !(sock->flags & GF_SOCK_NON_BLOCKING)) {
			gf_netcap_load_pck(nf, gf_sys_clock_high_res() );
			if (!nf->read_sock_selected) return nf->is_eos ? GF_IP_CONNECTION_CLOSED : GF_IP_NETWORK_EMPTY;

			if (nf->rt && (nf->pck_time > gf_sys_clock_high_res()))
				return GF_IP_NETWORK_EMPTY;
		}
		if (!buffer) return GF_OK;

		if (sock!=nf->read_sock_selected) return GF_IP_NETWORK_EMPTY;
		u32 to_read = length;
		if ((s32) to_read > nf->pck_len) to_read = nf->pck_len;
		u32 res = gf_bs_read_data(nf->cap_bs, buffer, to_read);
		if (nf->read_sock_selected->cap_info->patch_offset) {
			if (nf->read_sock_selected->cap_info->patch_offset-1<res) {
				buffer[nf->read_sock_selected->cap_info->patch_offset-1] = nf->read_sock_selected->cap_info->patch_val;
				nf->read_sock_selected->cap_info->patch_offset = 0;
			} else {
				nf->read_sock_selected->cap_info->patch_offset -= res;
			}
		}
		nf->pck_len -= res;
		if (!nf->pck_len) {
			nf->dst_port = 0;
			gf_netcap_load_pck(nf, 0);
		}

		if (BytesRead)
			*BytesRead = res;
		if (!res) return GF_IP_NETWORK_EMPTY;
		return GF_OK;
	}
#endif
	if (!sock->socket) return GF_BAD_PARAM;

	if (do_select && !(sock->flags & GF_SOCK_NON_BLOCKING)) {
		//check read
		GF_Err e = poll_select(sock, GF_SK_SELECT_READ, sock->usec_wait, GF_FALSE);
		if (e) return e;
	}
	if (!buffer) return GF_OK;

	if (sock->flags & GF_SOCK_HAS_PEER)
		res = (s32) recvfrom(sock->socket, (char *) buffer, length, 0, (struct sockaddr *)&sock->dest_addr, &sock->dest_addr_len);
	else {
		res = (s32) recv(sock->socket, (char *) buffer, length, 0);
		if (!do_select && (res == 0))
			return GF_IP_CONNECTION_CLOSED;
	}

	if (res == SOCKET_ERROR) {
		res = LASTSOCKERROR;
		switch (res) {
		case EAGAIN:
			return GF_IP_NETWORK_EMPTY;

#if defined(WIN32) || defined(_WIN32_WCE)
		//may happen if no select
		case WSAEWOULDBLOCK:
			return GF_IP_NETWORK_EMPTY;
#endif

#ifndef __SYMBIAN32__
		case EMSGSIZE:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] error reading: %s\n", gf_errno_str(LASTSOCKERROR)));
			return GF_OUT_OF_MEM;
		case ENOTCONN:
		case ECONNRESET:
		case ECONNABORTED:
#if defined(WIN32) || defined(_WIN32_WCE)
		case WSAECONNRESET:
#endif
			//log as debug, let higher level decide if this is an error or not
			GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[socket] error reading: %s\n", gf_errno_str(LASTSOCKERROR)));
			return GF_IP_CONNECTION_CLOSED;
#endif
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] error reading: %s\n", gf_errno_str(LASTSOCKERROR) ));
			return GF_IP_NETWORK_FAILURE;
		}
	}

#ifndef GPAC_DISABLE_NETCAP
	if (netcap_filter_pck(sock, res, GF_FALSE)) {
		res = 0;
	} else if (sock->cap_info && sock->cap_info->patch_offset) {
		buffer[sock->cap_info->patch_offset-1] = sock->cap_info->patch_val;
		sock->cap_info->patch_offset = 0;
	}
#endif

	if (!res) return GF_IP_NETWORK_EMPTY;

	if (BytesRead)
		*BytesRead = res;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sk_receive(GF_Socket *sock, u8 *buffer, u32 length, u32 *BytesRead)
{
	return gf_sk_receive_internal(sock, buffer, length, BytesRead, GF_TRUE);
}

GF_EXPORT
GF_Err gf_sk_receive_no_select(GF_Socket *sock, u8 *buffer, u32 length, u32 *BytesRead)
{
	return gf_sk_receive_internal(sock, buffer, length, BytesRead, GF_FALSE);
}

GF_EXPORT
GF_Err gf_sk_listen(GF_Socket *sock, u32 MaxConnection)
{
	s32 i;
	if (!sock || !sock->socket) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_NETCAP
	if (sock->cap_info && sock->cap_info->nf->cap_mode) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] Listening on socket is not supported when using network capture\n"));
		return GF_NOT_SUPPORTED;
	}
#endif

	if (!MaxConnection || (MaxConnection >= SOMAXCONN)) {
		MaxConnection = SOMAXCONN;
	}
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
	*newConnection = NULL;
	if (!sock || !(sock->flags & GF_SOCK_IS_LISTENING) ) return GF_BAD_PARAM;

	//check read
	GF_Err e = poll_select(sock, GF_SK_SELECT_READ, sock->usec_wait, GF_FALSE);
	if (e) return e;

#ifdef GPAC_HAS_IPV6
	client_address_size = sizeof(struct sockaddr_in6);
#else
	client_address_size = sizeof(struct sockaddr_in);
#endif
	sk = accept(sock->socket, (struct sockaddr *) &sock->dest_addr, &client_address_size);

	//we either have an error or we have no connections
	if (sk == INVALID_SOCKET) {
		switch (LASTSOCKERROR) {
		case EAGAIN:
			return GF_IP_NETWORK_EMPTY;
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] accept error: %s\n", gf_errno_str(LASTSOCKERROR)));
			return GF_IP_NETWORK_FAILURE;
		}
	}

	GF_SAFEALLOC((*newConnection), GF_Socket);
	(*newConnection)->socket = sk;
	(*newConnection)->flags = sock->flags & ~GF_SOCK_IS_LISTENING;
	(*newConnection)->usec_wait = sock->usec_wait;
#ifdef GPAC_HAS_IPV6
	memcpy( &(*newConnection)->dest_addr, &sock->dest_addr, client_address_size);
	memset(&sock->dest_addr, 0, sizeof(struct sockaddr_in6));
#else
	memcpy( &(*newConnection)->dest_addr, &sock->dest_addr, client_address_size);
	memset(&sock->dest_addr, 0, sizeof(struct sockaddr_in));
#endif

#ifdef SO_NOSIGPIPE
	int value = 1;
	setsockopt((*newConnection)->socket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
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
	char servname[NI_MAXHOST];
	struct sockaddr_in6 * addrptr = (struct sockaddr_in6 *)(&sock->dest_addr);
	if (!sock || !sock->socket) return GF_BAD_PARAM;
	my_inet_ntop(AF_INET, addrptr, clienthost, NI_MAXHOST);
	strcpy(buf, clienthost);
	if (getnameinfo((struct sockaddr *)addrptr, sock->dest_addr_len, clienthost, NI_MAXHOST, servname, NI_MAXHOST, NI_NUMERICHOST) == 0) {
		strcpy(buf, clienthost);
	}
#else
	if (!sock || !sock->socket) return GF_BAD_PARAM;
	strcpy(buf, inet_ntoa(sock->dest_addr.sin_addr));
#endif
	return GF_OK;
}



#if 0 //unused

//send length bytes of a buffer
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

	//the socket must be bound or connected
	if (!sock || !sock->socket) return GF_BAD_PARAM;
	if (remoteHost && !remotePort) return GF_BAD_PARAM;

	//check write
	GF_Err e = poll_select(sock, GF_SK_SELECT_WRITE, sock->usec_wait, GF_FALSE);
	if (e) return e;

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
		Host = gf_gethostbyname(remoteHost);
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
				return GF_IP_NETWORK_EMPTY;
			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[socket] sendto error: %s\n", gf_errno_str(LASTSOCKERROR)));
				return GF_IP_NETWORK_FAILURE;
			}
		}
		count += res;
	}
	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_sk_probe(GF_Socket *sock)
{
	s32 res;
	u8 buffer[1];
	if (!sock) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_NETCAP
	if (sock->cap_info && sock->cap_info->nf->read_socks && sock->cap_info->nf->is_eos) return GF_IP_CONNECTION_CLOSED;
#endif

	//check read - force using select at least for windows, for which poll returns nothing on connection abort
	GF_Err e = poll_select(sock, GF_SK_SELECT_READ, 100, GF_TRUE);
	if (e) return e;

	//peek message
	res = (s32) recv(sock->socket, buffer, 1, MSG_PEEK);
	if (res > 0) return GF_OK;
#if 0
	res = LASTSOCKERROR;
	switch (res) {
	case 0:
	case EAGAIN:
		return GF_IP_NETWORK_EMPTY;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[socket] probe error: %s\n", gf_errno_str(res)));
		return GF_IP_CONNECTION_CLOSED;
	}
#else
	return GF_IP_CONNECTION_CLOSED;
#endif

	return GF_OK;
}

#else
//for ntoh/hton
#include <arpa/inet.h>
#endif //GPAC_DISABLE_NETWORK


GF_EXPORT
u32 gf_htonl(u32 val)
{
	return htonl(val);
}


GF_EXPORT
u32 gf_ntohl(u32 val)
{
	return ntohl(val);
}

GF_EXPORT
u16 gf_htons(u16 val)
{
	return htons(val);
}


GF_EXPORT
u16 gf_ntohs(u16 val)
{
	return ntohs(val);
}

GF_EXPORT
const char *gf_errno_str(int errnoval)
{
	return strerror(errnoval);
}
