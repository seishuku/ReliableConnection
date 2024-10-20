#include "network.h"
#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#endif

const uint32_t Network_ReceiveBufferSize=1024*1024;
const uint32_t Network_SendBufferSize=1024*1024;

bool Network_Init(void)
{
#ifdef WIN32
	WSADATA WSAData;

	// Initialize Winsock
	if(WSAStartup(MAKEWORD(2, 2), &WSAData)!=0)
	{
		printf("Network_Init(): WSAStartup failed: %d\n", WSAGetLastError());
		return false;
	}
#endif

	return true;
}

void Network_Destroy(void)
{
#ifdef WIN32
	WSACleanup();
#endif
}

Socket_t Network_CreateSocket(void)
{
	Socket_t sock=(Socket_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(sock==-1)
	{
		printf("Network_CreateSocket() failed.\n");
		return -1;
	}

	if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *)&Network_ReceiveBufferSize, sizeof(uint32_t))==-1)
		printf("Network_CreateSocket() SO_RCVBUF set option failed.\n");

	if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char *)&Network_SendBufferSize, sizeof(uint32_t))==-1)
		printf("Network_CreateSocket() SO_SNDBUF set option failed.\n");

	// put socket in non-blocking mode
#ifdef WIN32
	u_long enabled=1;
	if(ioctlsocket(sock, FIONBIO, &enabled)==-1)
	{
		printf("Network_CreateSocket() FIONBIO enable failed.\n");
		return -1;
	}
#else
	int flags=fcntl(sock, F_GETFD, 0);
	if(fcntl(sock, F_SETFD, flags|O_NONBLOCK))
	{
		printf("Network_CreateSocket() O_NONBLOCK enable failed.\n");
		return -1;
	}
#endif

	return sock;
}

bool Network_SocketBind(const Socket_t sock, const uint32_t address, const uint16_t port)
{
	struct sockaddr_in sockaddr_in;

	sockaddr_in.sin_family=AF_INET;
	sockaddr_in.sin_addr.s_addr=htonl(address);
	sockaddr_in.sin_port=htons(port);

	if(bind(sock, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in))==-1)
	{
		printf("Network_SocketBind() failed.");
		return false;
	}

	return true;
}

bool Network_SocketSend(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, const uint32_t address, const uint16_t port)
{
	struct sockaddr_in server_address;

	server_address.sin_family=AF_INET;
	server_address.sin_addr.s_addr=htonl(address);
	server_address.sin_port=htons(port);

	if(sendto(sock, (const char *)buffer, buffer_size, 0, (struct sockaddr *)&server_address, sizeof(server_address))==-1)
	{
		printf("Network_SocketSend() failed.\n");
		return false;
	}

	return true;
}

int32_t Network_SocketReceive(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, uint32_t *address, uint16_t *port)
{
	struct sockaddr_in from;
	int32_t from_size=sizeof(from);
#ifdef WIN32
	int32_t bytes_received=recvfrom(sock, (char *)buffer, buffer_size, 0, (struct sockaddr *)&from, &from_size);
#else
	int32_t bytes_received=recvfrom(sock, (char *)buffer, buffer_size, MSG_DONTWAIT, (struct sockaddr *)&from, &from_size);
#endif

	*address=ntohl(from.sin_addr.s_addr);
	*port=ntohs(from.sin_port);

	return bytes_received;
}

bool Network_SocketClose(const Socket_t sock)
{
#ifdef WIN32
	if(closesocket(sock))
		return false;
#else
	if(close(sock))
		return false;
#endif

	return true;
}
