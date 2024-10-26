#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network.h"
#include "transport.h"

extern Socket_t netsocket;
double GetClock();

bool PacketSend(const Packet_t *frame)
{
	uint32_t address=NETWORK_ADDRESS(127, 0, 0, 1);
	uint16_t port=12345;

#if 0
	if(!Network_SocketSend(netsocket, frame->data, frame->len, address, port))
		return false;
#else
	if(!ReliableSocketSend(netsocket, frame->data, frame->len, address, port))
		return false;
#endif

	return true;
}

bool PacketReceive(Packet_t *frame)
{
	uint32_t address=0;
	uint16_t port=0;
#if 0
	int32_t length=Network_SocketReceive(netsocket, frame->data, PACKET_MAX_DATA+4, &address, &port);
#else
	int32_t length=ReliableSocketReceive(netsocket, frame->data, PACKET_MAX_DATA+4, &address, &port);
#endif

	if(length>0)
	{
		frame->len=length;
		return true;
	}

	return false;
}

void Transport_Send(const uint8_t *data, size_t len)
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
		PacketSend(&frame);
	}
	else
	{
		// Data is too big for single frame, send first frame here:
		sequence=1;

		frame.data[0]=0x10;					// 0x10 first frame flag
		frame.data[1]=(len>>8)&0xFF;		// Upper 8 bits of length
		frame.data[2]=len&0xFF;				// Lower 8 bits of length

		memcpy(&frame.data[3], data, PACKET_MAX_DATA-3);	// Copy data

		frame.len=PACKET_MAX_DATA;

		offset=PACKET_MAX_DATA-3;   // Track data sent

		// Send the frame
		PacketSend(&frame);

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
			PacketSend(&frame);
		}
	}
}

bool Transport_Receive(Transport_t *receiver)
{
	double timeout=GetClock()+1.0;

	memset(receiver, 0, sizeof(Transport_t));

	while(1)
	{
		Packet_t frame;

		memset(&frame, 0, sizeof(Packet_t));

		if(!PacketReceive(&frame))
		{
			if(GetClock()>timeout)
			{
				printf("Transport_Receive timed out.\n");
				return false;
			}

			continue;
		}

		// Determine the type of frame
		uint8_t pci_type=(frame.data[0]&0xF0)>>4;

		switch(pci_type)
		{
			case 0x0:	// Single frame
			{
				uint16_t len=((frame.data[1]<<8)|frame.data[2])&0xFFFF;

				receiver->buffer=(uint8_t *)malloc(len);

				if(receiver->buffer==NULL)
					return false;

				memcpy(receiver->buffer, &frame.data[3], len);

				receiver->length=len;
				receiver->offset=len;
				break;
			}

			case 0x1:	// First frame
			{
				uint16_t len=((frame.data[1]<<8)|frame.data[2])&0xFFFF;

				receiver->buffer=(uint8_t *)malloc(len);

				if(receiver->buffer==NULL)
					return false;

				memcpy(receiver->buffer, &frame.data[3], PACKET_MAX_DATA-3);

				receiver->length=len;
				receiver->offset=PACKET_MAX_DATA-3;
				receiver->sequence=1;
				break;
			}

			case 0x2:	// Consecutive frame
			{
				uint8_t seq_num=frame.data[1];

				// Sequence number mismatch
				if(seq_num!=receiver->sequence)
					return 0;

				size_t bytes_to_copy=receiver->length-receiver->offset<PACKET_MAX_DATA-2?receiver->length-receiver->offset:PACKET_MAX_DATA-2;

				memcpy(&receiver->buffer[receiver->offset], &frame.data[2], bytes_to_copy);

				receiver->offset+=bytes_to_copy;
				receiver->sequence++;
				break;
			}

			default:	// Unknown frame type
				return false; // Error
		}

		// Check if the message is fully received
		if(receiver->offset>=receiver->length)
			return true;
	}

	return false;
}