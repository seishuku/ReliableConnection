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

// TODO: Need a more independant timing function for timeout calcs
double GetClock(void);

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

static const uint32_t ACK_MAGIC='A'|'c'<<8|'k'<<16|'\0'<<24;
static const uint32_t RETRY_MAGIC='R'|'e'<<8|'t'<<16|'y'<<24;
static const uint32_t MAX_RETRIES=3;
static const double TIMEOUT=1.25;

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
	const uint32_t poly=0xEDB88320;
	crc=~crc;

	while(len--)
	{
		crc^=*buf++;
		crc=crc&1?(crc>>1)^poly:crc>>1;
		crc=crc&1?(crc>>1)^poly:crc>>1;
		crc=crc&1?(crc>>1)^poly:crc>>1;
		crc=crc&1?(crc>>1)^poly:crc>>1;
		crc=crc&1?(crc>>1)^poly:crc>>1;
		crc=crc&1?(crc>>1)^poly:crc>>1;
		crc=crc&1?(crc>>1)^poly:crc>>1;
		crc=crc&1?(crc>>1)^poly:crc>>1;
	}

	return ~crc;
}

// Send will add the CRC onto the end of the buffer, the buffer must be your size + size of UINT32, so be sure to size accordingly.
// Similarly receive does the same, but in reverse, reads off the buffer past the specified length.
int32_t ReliableSocketReceive(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, uint32_t *address, uint16_t *port, const double timeout)
{
	// Set up max retry count
	int32_t tries=MAX_RETRIES;

	while(tries>0)
	{
		// Set timeout at current time + timeout const
		const double time=GetClock()+timeout;

		while(1)
		{
			const int32_t length=Network_SocketReceive(sock, buffer, buffer_size, address, port);

			if(length>0)
			{
				// Extract the CRC calculated by the sender
				const uint32_t crcRecv=*(uint32_t *)(buffer+length-sizeof(uint32_t));
				// Calculate the CRC of what was received
				const uint32_t crc=crc32c(0, buffer, length-sizeof(uint32_t));

				// Same?
				if(crc!=crcRecv)
				{
					printf("ReliableSocketReceive: CRC mismatch, retrying.\n");

					// Send retry
					const uint32_t retry_magic=RETRY_MAGIC;
					if(!Network_SocketSend(sock, (uint8_t *)&retry_magic, sizeof(uint32_t), *address, *port))
					{
						printf("ReliableSocketReceive: Retry send failed.\n");
						return -1;
					}

					tries--;
					break;
				}

				// Send acknowledgement
				const uint32_t ack_magic=ACK_MAGIC;
				if(!Network_SocketSend(sock, (uint8_t *)&ack_magic, sizeof(uint32_t), *address, *port))
				{
					printf("ReliableSocketReceive: Acknowledgement send failed.\n");
					return -1;
				}

				// We're done, return buffer size less the CRC
				return length-sizeof(uint32_t);
			}

			// Check if timed out
			if(GetClock()>time)
			{
				printf("ReliableSocketReceive: Timed out.\n");
				return -1;
			}
		}
	}

	printf("ReliableSocketReceive: Too many retries.\n");
	return -1;
}

bool ReliableSocketSend(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, const uint32_t address, const uint16_t port, const double timeout)
{
	// Set up max retry count
	int32_t tries=MAX_RETRIES;

	// Calculate CRC and attach it to the end of the buffer
	(*(uint32_t *)(buffer+buffer_size))=crc32c(0, buffer, buffer_size);

	while(tries>0)
	{
		// Set timeout at current time + timeout const
		const double time=GetClock()+timeout;

		// Initial data send
		if(Network_SocketSend(sock, buffer, buffer_size+sizeof(uint32_t), address, port))
		{
			// Read socket for incoming response
			while(1)
			{
				uint32_t sAddress=0, ack=0;
				uint16_t sPort=0;
				int32_t length=Network_SocketReceive(sock, (uint8_t *)&ack, sizeof(uint32_t), &sAddress, &sPort);

				// Got some data
				if(length>0)
				{
					// Check for address and port match (is this really needed?)
					if(sAddress==address&&sPort==port)
					{
						// Check response
						if(ack==ACK_MAGIC)	// All good
							return true;
						else if(ack==RETRY_MAGIC)	// Retry requested, resend buffer
						{
							// Resend data
							if(!Network_SocketSend(sock, buffer, buffer_size+sizeof(uint32_t), address, port))
							{
								printf("ReliableSocketSend: Network_SocketSend failed.\n");
								return false;
							}

							printf("ReliableSocketSend: Retry.\n");
							tries--;
							break;
						}
					}
					else
					{
						printf("ReliableSocketSend: Address/port mismatch on acknowledgement.\n");
						return false;
					}

					return true;
				}

				// Check if timed out
				if(GetClock()>time)
				{
					printf("ReliableSocketSend: Timed out.\n");
					return false;
				}
			}
		}
		else
		{
			printf("ReliableSocketSend: Network_SocketSend failed.\n");
			return false;
		}
	}

	printf("ReliableSocketSend: Too many retries.\n");
	return false;
}
