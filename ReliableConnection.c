#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "network.h"

static bool server=false;
static Socket_t socket=-1;

#define BUFFER_SIZE 32767
static uint8_t *buffer=NULL;

static const uint32_t ACK_MAGIC='A'|'c'<<8|'k'<<16|'\0'<<24;
static const uint32_t RETRY_MAGIC='R'|'e'<<8|'t'<<16|'y'<<24;

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <conio.h>
double GetClock(void)
{
	static uint64_t frequency=0;
	uint64_t count;

	if(!frequency)
		QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);

	QueryPerformanceCounter((LARGE_INTEGER *)&count);

	return (double)count/frequency;
}
#else
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

double GetClock(void)
{
	struct timespec ts;

	if(!clock_gettime(CLOCK_MONOTONIC, &ts))
		return ts.tv_sec+(double)ts.tv_nsec/1000000000.0;

	return 0.0;
}

// kbhit for *nix
int _kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
	newt=oldt;
	newt.c_lflag&=~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf=fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf|O_NONBLOCK);

	ch=getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if(ch!=EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}
#endif

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

int32_t ReliableSocketReceive(const Socket_t sock, uint8_t *buffer, const uint32_t buffer_size, uint32_t *address, uint16_t *port)
{
	int32_t retries=3;

	while(retries>0)
	{
		// Set timeout at current time +1.25 seconds
		double timeout=GetClock()+1.25;

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

					retries--;
					break;
				}

				// Send acknowledgement
				const uint32_t ack_magic=ACK_MAGIC;
				if(!Network_SocketSend(sock, (uint8_t *)&ack_magic, sizeof(uint32_t), *address, *port))
				{
					printf("ReliableSocketReceive: Acknowledgement send failed.\n");
					return -1;
				}

				return length-sizeof(uint32_t);
			}

			if(GetClock()>timeout)
			{
				printf("ReliableSocketReceive: Timed out.\n");
				return -1;
			}
		}
	}

	printf("ReliableSocketReceive: Too many retries.\n");
	return -1;
}

bool ReliableSocketSend(const Socket_t sock, uint8_t *buffer, uint32_t buffer_size, uint32_t address, uint16_t port)
{
	int32_t retries=3;

	// Calculate CRC and attach it to the end of the buffer
	(*(uint32_t *)(buffer+buffer_size))=crc32c(0, buffer, buffer_size);

	while(retries>0)
	{
		double timeout=GetClock()+1.25;

		if(Network_SocketSend(sock, buffer, buffer_size+sizeof(uint32_t), address, port))
		{
			while(1)
			{
				uint32_t sAddress=0, ack=0;
				uint16_t sPort=0;
				int32_t length=Network_SocketReceive(sock, (uint8_t *)&ack, sizeof(uint32_t), &sAddress, &sPort);

				if(length>0)
				{
					if(sAddress==address&&sPort==port)
					{
						if(ack==ACK_MAGIC)
							return true;
						else if(ack==RETRY_MAGIC)
						{
							if(!Network_SocketSend(sock, buffer, buffer_size+sizeof(uint32_t), address, port))
							{
								printf("ReliableSocketSend: Network_SocketSend failed.\n");
								return false;
							}

							printf("ReliableSocketSend: Retry.\n");
							retries--;
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

				if(GetClock()>timeout)
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

int main(int argc, char **argv)
{
	if(argc>1&&strncmp("-server", argv[1], 7)==0)
	{
		printf("Server mode\n");
		server=true;
	}
	else
		printf("Client mode\n");

	buffer=(uint8_t *)malloc(BUFFER_SIZE);

	if(buffer==NULL)
	{
		printf("Failed to allocate buffer.\n");
		return -1;
	}

	if(!Network_Init())
	{
		printf("Network initialization failed.\n");
		return -1;
	}

	socket=Network_CreateSocket();

	if(socket!=-1)
		printf("Created socket: %d\n", socket);
	else
		return -1;

	if(server)
	{
		if(!Network_SocketBind(socket, NETWORK_ADDRESS(0, 0, 0, 0), 12345))
		{
			printf("Failed to bind socket.\n");
			return -1;
		}

		uint32_t address=0;
		uint16_t port=0;

		while(!_kbhit())
		{
			int32_t length=ReliableSocketReceive(socket, buffer, BUFFER_SIZE, &address, &port);

			if(length>0)
				printf("Got %d bytes on %d from %X. (%llu)\n", length, port, address, *(uint64_t *)buffer);
		}
	}
	else
	{
		uint64_t var=0;
		uint32_t count=0;
		double avgTime=0.0;

		while(!_kbhit())
		{
			uint32_t address=NETWORK_ADDRESS(172, 26, 218, 132);
			uint16_t port=12345;

			var++;
			memcpy(buffer, &var, sizeof(uint64_t));

			double start=GetClock();

			if(ReliableSocketSend(socket, buffer, sizeof(uint64_t), address, port))
				avgTime+=(GetClock()-start)*1000.0;

			if(count++>1000)
			{
				printf("Took %lfms.                    \r", avgTime/count);
				count=0;
				avgTime=0.0;
			}
		}
	}

	Network_SocketClose(socket);
	Network_Destroy();

	free(buffer);

	return 0;
}
