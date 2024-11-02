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
	uint8_t sequence=0;
	Packet_t frame;

	if(len<=(PACKET_MAX_DATA-3))
	{
		// Data size is small enough to fit in one frame, so send single frame:
		frame.data[0]=0x00;					// 0x00 single frame flag
		frame.data[1]=(len>>8)&0xFF;		// Upper 8 bits of length
		frame.data[2]=len&0xFF;				// Lower 8 bits of length
		memcpy(&frame.data[3], data, len);	// Copy data

		frame.len=len+3;

		// Send the frame
		if(!PacketSend(&frame, sock, address, port, timeout))
			return false;
	}
	else
	{
		// Data is too big for single frame, send first frame here:
		sequence=1;

		frame.data[0]=0x10;									// 0x10 first frame flag
		frame.data[1]=(len>>8)&0xFF;						// Upper 8 bits of length
		frame.data[2]=len&0xFF;								// Lower 8 bits of length
		memcpy(&frame.data[3], data, PACKET_MAX_DATA-3);	// Copy data

		frame.len=PACKET_MAX_DATA;

		offset=PACKET_MAX_DATA-3;   // Track data sent

		// Send the frame
		if(!PacketSend(&frame, sock, address, port, timeout))
			return false;

		while(offset<len)
		{
			frame.data[0]=0x20;		// 0x20 consecutive frame flag
			frame.data[1]=sequence;	// Sequence number

			size_t bytes_to_copy=(len-offset)<PACKET_MAX_DATA-2?(len-offset):PACKET_MAX_DATA-2;
			memcpy(&frame.data[2], data+offset, bytes_to_copy);

			frame.len=bytes_to_copy+2;

			sequence++;
			offset+=bytes_to_copy;

			// Send the frame
			if(!PacketSend(&frame, sock, address, port, timeout))
				return false;
		}
	}

	return true;
}

uint8_t *Transport_Receive(const Socket_t sock, size_t *length, uint32_t *address, uint16_t *port, const double timeout)
{
	uint8_t *buffer=NULL;
	size_t offset=0;
	uint8_t sequence=0;
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
		uint8_t pci_type=frame.data[0];

		switch(pci_type)
		{
			case 0x00:	// Single frame
			{
				uint16_t len=((frame.data[1]<<8)|frame.data[2])&0xFFFF;

				if(len)
					buffer=(uint8_t *)malloc(len);

				if(buffer==NULL)
				{
					*length=0;
					return NULL;
				}

				memcpy(buffer, &frame.data[3], len);

				*length=len;
				offset=len;
				break;
			}

			case 0x10:	// First frame
			{
				uint16_t len=((frame.data[1]<<8)|frame.data[2])&0xFFFF;

				if(len)
					buffer=(uint8_t *)malloc(len);

				if(buffer==NULL)
				{
					*length=0;
					return NULL;
				}

				memcpy(buffer, &frame.data[3], PACKET_MAX_DATA-3);

				*length=len;
				offset=PACKET_MAX_DATA-3;
				sequence=1;
				break;
			}

			case 0x20:	// Consecutive frame
			{
				uint8_t seq_num=frame.data[1];

				// Sequence number mismatch
				if(seq_num!=sequence)
				{
					*length=0;
					return NULL;
				}

				size_t bytes_to_copy=*length-offset<PACKET_MAX_DATA-2?*length-offset:PACKET_MAX_DATA-2;

				memcpy(&buffer[offset], &frame.data[2], bytes_to_copy);

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