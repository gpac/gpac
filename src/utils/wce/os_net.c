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

#include <winsock.h>

#define SOCK_MICROSEC_WAIT	500


#define EAGAIN					WSAEWOULDBLOCK
#define EISCONN				WSAEISCONN
#define ENOTCONN			WSAENOTCONN
#define ECONNRESET		WSAECONNRESET
#define EMSGSIZE			WSAEMSGSIZE

#define LASTSOCKERROR WSAGetLastError()

/* socket status */
#define SK_STATUS_CREATE	0x01
#define SK_STATUS_BIND		0x02
#define SK_STATUS_CONNECT	0x03
#define SK_STATUS_LISTEN	0x04


struct __tag_socket
{
	u32 status;
	SOCKET socket;
	u32 type;
	Bool blocking;
	//this is used for server sockets / multicast sockets
	struct sockaddr_in RemoteAddress;
};

//the number of sockets used. This because the WinSock lib needs init
static u32 IsInit = 0;


/*
		Some NTP tools
*/

void gf_get_ntp(u32 *sec, u32 *frac)
{
	s32 gettimeofday(struct timeval *tp, void *tz);
	struct timeval now;
	gettimeofday(&now, NULL);
	*sec = now.tv_sec + GF_NTP_SEC_1900_TO_1970;
	*frac = (now.tv_usec << 12) + (now.tv_usec << 8) - ((now.tv_usec * 3650) >> 6);
}



GF_Err gf_sk_get_host_name(char *buffer)
{
	s32 ret;
	ret = gethostname(buffer, GF_MAX_IP_NAME_LEN);

	if (ret == SOCKET_ERROR) return GF_IP_ADDRESS_NOT_FOUND;
	return GF_OK;
}

GF_Err gf_sk_get_local_ip(GF_Socket *sock, char *buffer)
{
	struct sockaddr_in name;
	char *ip;
	u32 len = sizeof(struct sockaddr_in);

	buffer[0] = 0;
	if (getsockname(sock->socket, (struct sockaddr*) &name, &len)) return GF_IP_NETWORK_FAILURE;

	ip = inet_ntoa(name.sin_addr);
	if (!ip) return GF_IP_NETWORK_FAILURE;
	sprintf(buffer, ip);
	return GF_OK;
}

GF_Socket *gf_sk_new(u32 SocketType)
{
	GF_Socket *tmp;
	WSADATA Data;
	//init WinSock
	if (!IsInit)
		if (WSAStartup(0x0202, &Data ) != 0 ) return NULL;

	if ((SocketType != GF_SOCK_TYPE_UDP) && (SocketType != GF_SOCK_TYPE_TCP)) return NULL;

	tmp = malloc(sizeof(GF_Socket));
	memset(tmp, 0, sizeof(GF_Socket));

	tmp->socket = socket(AF_INET, (SocketType == GF_SOCK_TYPE_UDP) ? SOCK_DGRAM : SOCK_STREAM, 0);
	if (tmp->socket == INVALID_SOCKET) {
		free(tmp);
		if (!IsInit) WSACleanup();
		return NULL;
	}

	tmp->type = SocketType;
	tmp->status = SK_STATUS_CREATE;
	IsInit ++;
	tmp->blocking = 1;

	memset(&tmp->RemoteAddress, 0, sizeof(struct sockaddr_in));

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
	res = ioctlsocket(sock->socket, FIONBIO, &NonBlockingOn);
	if (res) return GF_SERVICE_ERROR;
	sock->blocking = NonBlockingOn ? 0 : 1;
	return GF_OK;
}


void gf_sk_del(GF_Socket *sock)
{
	closesocket(sock->socket);
	IsInit --;
	if (!IsInit) WSACleanup();
	free(sock);
}

void gf_sk_reset(GF_Socket *sock)
{
	u32 clear;

	if (!sock) return;
	//clear the socket buffer and state
	setsockopt(sock->socket, SOL_SOCKET, SO_ERROR, (char *) &clear, sizeof(u32) );

}

s32 gf_sk_get_handle(GF_Socket *sock) 
{
	return sock->socket;
}

//connects a socket to a remote peer on a given port
GF_Err gf_sk_connect(GF_Socket *sock, char *PeerName, u16 PortNumber)
{
	s32 ret;
	struct hostent *Host;

	//setup the address
	sock->RemoteAddress.sin_family = AF_INET;
	sock->RemoteAddress.sin_port = htons(PortNumber);
	sock->RemoteAddress.sin_addr.s_addr = inet_addr(PeerName);
	if (sock->RemoteAddress.sin_addr.s_addr==INADDR_NONE) {
		Host = gethostbyname(PeerName);
		if (Host == NULL) return GF_IP_ADDRESS_NOT_FOUND;
		memcpy((char *) &sock->RemoteAddress.sin_addr, Host->h_addr_list[0], sizeof(u32));
	}
	if (sock->type == GF_SOCK_TYPE_TCP) {
		ret = connect(sock->socket, (struct sockaddr *) &sock->RemoteAddress, sizeof(struct sockaddr));
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
	sock->status = SK_STATUS_CONNECT;
	return GF_OK;
}

//binds the given socket to the specified port. If ReUse is true
//this will enable reuse of ports on a single machine
GF_Err gf_sk_bind(GF_Socket *sock, u16 PortNumber, Bool reUse)
{
	s32 ret;
	s32 optval;
	char buf[GF_MAX_IP_NAME_LEN];
	struct sockaddr_in LocalAdd;
	struct hostent *Host;

	if (!sock || (sock->status != SK_STATUS_CREATE)) return GF_BAD_PARAM;

	memset((void *) &LocalAdd, 0, sizeof(LocalAdd));
	//ger the local name
	ret = gethostname(buf, GF_MAX_IP_NAME_LEN);
	if (ret == SOCKET_ERROR) return GF_IP_ADDRESS_NOT_FOUND;
	//get the IP address
	Host = gethostbyname(buf);
	if (Host == NULL) return GF_IP_ADDRESS_NOT_FOUND;
	//setup the address
	memcpy((char *) &LocalAdd.sin_addr, Host->h_addr_list[0], sizeof(u32));
	LocalAdd.sin_family = AF_INET;
	LocalAdd.sin_port = htons(PortNumber);

	if (reUse) {
		//retry with ReUsability of socket names
		optval = SO_REUSEADDR;
		setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));
	}

	//bind the socket
	ret = bind(sock->socket, (struct sockaddr *) &LocalAdd, sizeof(LocalAdd));
	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
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
	if (sock->status != SK_STATUS_CONNECT) return GF_BAD_PARAM;

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
			Res = sendto(sock->socket, &buffer[Count], length - Count, 0, (struct sockaddr *) &sock->RemoteAddress, sizeof(struct sockaddr));
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
	if (!multi_IPAdd) return 0;
	return ((htonl(inet_addr(multi_IPAdd)) >> 8) & 0x00f00000) == 0x00e00000;	
}

//binds MULTICAST
GF_Err gf_sk_setup_multicast(GF_Socket *sock, char *multi_IPAdd, u16 MultiPortNumber, u32 TTL, Bool NoBind, char *local_interface_ip)
{
	s32 ret;
	DWORD optval;
	u_long mc_add, local_add;
	u32 flag; 
	SOCKADDR_IN LocalAdd;
	struct ip_mreq M_req;

	if (!sock || (sock->status != SK_STATUS_CREATE)) return GF_BAD_PARAM;

	if (TTL > 255) return GF_BAD_PARAM;

	memset((void *) &LocalAdd, 0, sizeof(LocalAdd));

	//check the address
	mc_add = inet_addr(multi_IPAdd);
	if (!((mc_add >= 0xe0000000) || (mc_add <= 0xefffffff) )) return GF_BAD_PARAM;

	//retry with ReUsability of socket names
	optval = SO_REUSEADDR;
	setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval));

	if (local_interface_ip) local_add = inet_addr(local_interface_ip);
	else local_add = INADDR_ANY;

	//bind to ANY interface WITHOUT port number
	LocalAdd.sin_family = AF_INET;
	LocalAdd.sin_addr.s_addr = local_add;
	LocalAdd.sin_port = htons( MultiPortNumber);

	if (!NoBind) {
		//bind the socket
		ret = bind(sock->socket, (struct sockaddr *) &LocalAdd, sizeof(LocalAdd));
		if (ret == SOCKET_ERROR) {
			/*retry without specifying the local add*/
			LocalAdd.sin_addr.s_addr = local_add = INADDR_ANY;
			local_interface_ip = NULL;
			ret = bind(sock->socket, (struct sockaddr *) &LocalAdd, sizeof(LocalAdd));
			if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
		}
	}
	sock->status = SK_STATUS_BIND;
 
	//setup local interface
	if (local_interface_ip) {
		ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_IF, (char *) &local_add, sizeof(local_add));
		if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;
	}

	//now join the multicast
	M_req.imr_multiaddr.s_addr = mc_add;
	//ANY interfaces for now
	M_req.imr_interface.s_addr = local_add;
	ret = setsockopt(sock->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &M_req, sizeof(M_req));

	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;

	//set the Time To Live
	ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&TTL, sizeof(TTL));
	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;

	//Disable loopback
	flag = 1;
	ret = setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &flag, sizeof(flag));
	if (ret == SOCKET_ERROR) return GF_IP_CONNECTION_FAILURE;

	sock->RemoteAddress.sin_family = AF_INET;
	sock->RemoteAddress.sin_addr.s_addr = mc_add;
	sock->RemoteAddress.sin_port = htons( MultiPortNumber);
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
			return GF_IP_NETWORK_FAILURE;
		}
	}
	if (!FD_ISSET(sock->socket, &Group)) {
		return GF_IP_NETWORK_EMPTY;
	}

	res = recv(sock->socket, buffer + startFrom, length - startFrom, 0);
	if (res == SOCKET_ERROR) {
		switch (LASTSOCKERROR) {
		case EMSGSIZE:
			return GF_OUT_OF_MEM;
		case EAGAIN:
			return GF_IP_SOCK_WOULD_BLOCK;
		case ENOTCONN:
		case ECONNRESET:
			return GF_IP_CONNECTION_CLOSED;
		default:
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
	u32 clientAddSize, ready, res;
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

	clientAddSize = sizeof(struct sockaddr_in);
	sk = accept(sock->socket, (struct sockaddr *) &sock->RemoteAddress, &clientAddSize);

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
	memcpy( &(*newConnection)->RemoteAddress, &sock->RemoteAddress, clientAddSize);

	memset(&sock->RemoteAddress, 0, sizeof(struct sockaddr_in));

	return GF_OK;
}

GF_Err gf_sk_get_local_info(GF_Socket *sock, u16 *Port, u32 *Familly)
{
	struct sockaddr_in the_add;
	u32 size, fam;

	*Port = 0;
	*Familly = 0;

	if (!sock || sock->status != SK_STATUS_CONNECT) return GF_BAD_PARAM;

	size = sizeof(struct sockaddr_in);
	if (getsockname(sock->socket, (struct sockaddr *) &the_add, &size) == SOCKET_ERROR) return GF_IP_NETWORK_FAILURE;
	*Port = (u32) ntohs(the_add.sin_port);

	size = 4;
	if (getsockopt(sock->socket, SOL_SOCKET, SO_TYPE, (char *) &fam, &size) == SOCKET_ERROR)
		return GF_IP_NETWORK_FAILURE;

	switch (fam) {
	case SOCK_DGRAM:
		*Familly = GF_SOCK_TYPE_UDP;
		return GF_OK;
	case SOCK_STREAM:
		*Familly = GF_SOCK_TYPE_TCP;
		return GF_OK;
	default:
		*Familly = 0;
		return GF_OK;
	}
}

//we have to do this for the server sockets as we use only one thread 
GF_Err gf_sk_server_mode(GF_Socket *sock, Bool serverOn)
{
	u32 one;

	if (!sock 
		|| (sock->type != GF_SOCK_TYPE_TCP)
		|| (sock->status != SK_STATUS_CONNECT)
		)
		return GF_BAD_PARAM;

	one = serverOn ? 1 : 0;
	setsockopt(sock->socket, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof(u32));
	setsockopt(sock->socket, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(u32));
	return GF_OK;
}

GF_Err gf_sk_get_remote_address(GF_Socket *sock, char *buf)
{
	if (!sock || sock->status != SK_STATUS_CONNECT) return GF_BAD_PARAM;
	sprintf(buf, inet_ntoa(sock->RemoteAddress.sin_addr));
	return GF_OK;	
}


GF_Err gf_sk_set_remote_address(GF_Socket *sock, char *address)
{
	struct hostent *Host;

	if (!sock || !address) return GF_BAD_PARAM;

	//setup the address
	sock->RemoteAddress.sin_family = AF_INET;

	sock->RemoteAddress.sin_addr.s_addr = inet_addr(address);
	if (sock->RemoteAddress.sin_addr.s_addr==INADDR_NONE) {
		Host = gethostbyname(address);
		if (Host == NULL) return GF_IP_ADDRESS_NOT_FOUND;
		memcpy((char *) &sock->RemoteAddress.sin_addr, Host->h_addr_list[0], sizeof(u32));
	}
	return GF_OK;
}

GF_Err gf_sk_set_remote_port(GF_Socket *sock, u16 RemotePort)
{
	if (!sock || !RemotePort) return GF_BAD_PARAM;
	sock->RemoteAddress.sin_port = htons(RemotePort);
	return GF_OK;
}


//send length bytes of a buffer
GF_Err gf_sk_send_to(GF_Socket *sock, unsigned char *buffer, u32 length, unsigned char *remoteHost, u16 remotePort)
{
	u32 Count, Res, ready;
	struct sockaddr_in remote;
	struct hostent *Host;
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

	//setup the address
	remote.sin_family = AF_INET;
	//if a remote host is specified, use it. Otherwise use the default host
	if (remoteHost) {
		//setup the address
		remote.sin_port = htons(remotePort);
		//get the server IP
		Host = gethostbyname(remoteHost);
		if (Host == NULL) return GF_IP_ADDRESS_NOT_FOUND;
		memcpy((char *) &remote.sin_addr, Host->h_addr_list[0], sizeof(u32));
	} else {
		remote.sin_port = sock->RemoteAddress.sin_port;
		remote.sin_addr.s_addr = sock->RemoteAddress.sin_addr.s_addr;
	}
	
	Count = 0;
	while (Count < length) {
		Res = sendto(sock->socket, &buffer[Count], length - Count, 0, (struct sockaddr *) &remote, sizeof(remote));
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


