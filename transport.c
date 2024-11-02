#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "transport.h"

double GetClock();

#define PACKET_MAX_DATA 1024

typedef struct
{
	uint8_t data[PACKET_MAX_DATA+4]; // Max size + CRC
	size_t len;
} Packet_t;

static bool PacketSend(const Packet_t *frame, const Socket_t sock, const uint32_t address, const uint16_t port, const double timeout)
{
#if 0
	if(!Network_SocketSend(sock, frame->data, frame->len, address, port))
		return false;
#else
	if(!ReliableSocketSend(sock, frame->data, frame->len, address, port, timeout))
		return false;
#endif

	return true;
}

static bool PacketReceive(Packet_t *frame, const Socket_t sock, uint32_t *address, uint16_t *port, const double timeout)
{
	memset(frame, 0, sizeof(Packet_t));

#if 0
	int32_t length=Network_SocketReceive(sock, frame->data, PACKET_MAX_DATA+4, address, port);
#else
	int32_t length=ReliableSocketReceive(sock, frame->data, PACKET_MAX_DATA+4, address, port, timeout);
#endif

	if(length>0)
	{
		frame->len=length;
		return true;
	}

	return false;
}

bool Transport_Send(const Socket_t sock, const uint8_t *data, const size_t len, const uint32_t address, const uint16_t port, const double timeout)
{
	size_t offset=0;
	uint32_t sequence=1;
	Packet_t frame;

	if(len<=(PACKET_MAX_DATA-sizeof(uint32_t)))
	{
		// Data size is small enough to fit in one frame, so send single frame:
		// Upper 30bits for size, lower 4bits for frame type
		uint32_t header=((len&0xFFFFFFF)<<4)|0;
		memcpy(frame.data, &header, sizeof(uint32_t));	// Copy header
		memcpy(frame.data+sizeof(uint32_t), data, len);	// Copy data

		frame.len=len+sizeof(uint32_t);

		// Send the frame
		if(!PacketSend(&frame, sock, address, port, timeout))
			return false;
	}
	else
	{
		// Data is too big for single frame, send first frame here:
		// Upper 30bits for size, lower 4bits for frame type
		uint32_t header=((len&0xFFFFFFF)<<4)|1;
		memcpy(frame.data, &header, sizeof(uint32_t));									// Copy header
		memcpy(frame.data+sizeof(uint32_t), data, PACKET_MAX_DATA-sizeof(uint32_t));	// Copy data

		frame.len=PACKET_MAX_DATA;

		// Send the frame
		if(!PacketSend(&frame, sock, address, port, timeout))
			return false;

		offset=PACKET_MAX_DATA-sizeof(uint32_t);   // Track data sent

		while(offset<len)
		{
			size_t bytes_to_copy=(len-offset)<PACKET_MAX_DATA-sizeof(uint32_t)?(len-offset):PACKET_MAX_DATA-sizeof(uint32_t);

			uint32_t header=((sequence&0xFFFFFFF)<<4)|2;
			memcpy(frame.data, &header, sizeof(uint32_t));		// Copy header
			memcpy(frame.data+sizeof(uint32_t), data+offset, bytes_to_copy);	// Copy data

			frame.len=bytes_to_copy+sizeof(uint32_t);

			// Send the frame
			if(!PacketSend(&frame, sock, address, port, timeout))
				return false;

			sequence++;
			offset+=bytes_to_copy;
		}
	}

	return true;
}

uint8_t *Transport_Receive(const Socket_t sock, size_t *length, uint32_t *address, uint16_t *port, const double timeout)
{
	uint8_t *buffer=NULL;
	size_t offset=0;
	uint32_t sequence=1;
	double time=GetClock()+timeout;

	*length=0;

	while(1)
	{
		Packet_t frame;

		memset(&frame, 0, sizeof(Packet_t));

		if(!PacketReceive(&frame, sock, address, port, timeout))
		{
			if(GetClock()>time)
			{
				printf("Transport_Receive timed out.\n");

				*length=0;
				return NULL;
			}

			continue;
		}

		// Determine the type of frame
		uint32_t header=0;
		memcpy(&header, frame.data, sizeof(uint32_t));

		switch(header&0xF)
		{
			case 0:	// Single frame
			{
				uint32_t len=header>>4;

				if(len)
					buffer=(uint8_t *)malloc(len);

				if(buffer==NULL)
				{
					*length=0;
					return NULL;
				}

				memcpy(buffer, frame.data+sizeof(uint32_t), len);

				*length=len;
				offset=len;
				break;
			}

			case 1:	// First frame
			{
				uint32_t len=header>>4;

				if(len)
					buffer=(uint8_t *)malloc(len);

				if(buffer==NULL)
				{
					*length=0;
					return NULL;
				}

				memcpy(buffer, frame.data+sizeof(uint32_t), PACKET_MAX_DATA-sizeof(uint32_t));

				*length=len;
				offset=PACKET_MAX_DATA-sizeof(uint32_t);
				break;
			}

			case 2:	// Consecutive frame
			{
				uint32_t seq_num=header>>4;

				// Sequence number mismatch
				if(seq_num!=sequence)
				{
					*length=0;
					return NULL;
				}

				size_t bytes_to_copy=*length-offset<PACKET_MAX_DATA-sizeof(uint32_t)?*length-offset:PACKET_MAX_DATA-sizeof(uint32_t);

				memcpy(buffer+offset, frame.data+sizeof(uint32_t), bytes_to_copy);

				offset+=bytes_to_copy;
				sequence++;
				break;
			}

			default:	// Unknown frame type
				*length=0;
				return NULL;
		}

		// Check if the message is fully received
		if(offset>=*length)
			return buffer;
	}

	*length=0;
	return NULL;
}
