#include <stdio.h>
#include <string.h>
#include "network.h"
#include "transport.h"

extern Socket_t netsocket;
double GetClock();

/// <summary>
/// sender stuff
/// </summary>
bool PacketSend(const Packet_t *frame)
{
	uint32_t address=NETWORK_ADDRESS(127, 0, 0, 1);
	uint16_t port=12345;

	if(!Network_SocketSend(netsocket, frame->data, frame->len, address, port))
		return false;

	return true;
}

void Transport_Send(const uint8_t *data, size_t len)
{
	size_t offset=0;
	uint8_t sequence=0;
	Packet_t frame;

	if(len<=TRANSPORT_PAYLOAD_SIZE)
	{
		// Data size is small enough to fit in one frame, so send single frame:
		frame.data[0]=0x00|(len&0x0F);		// 0x00 single frame flag + upper 4 bits of length
		memcpy(&frame.data[1], data, len);	// Copy data

		frame.len=len+1;

		// Send the frame
		PacketSend(&frame);
	}
	else
	{
		// Data is too big for single frame, send first frame here:
		sequence=1;

		frame.data[0]=0x10|((len>>8)&0x0F);		// 0x10 first frame flag + upper 4 bits of length
		frame.data[1]=len&0xFF;					// Lower 8 bits of Length

		memcpy(&frame.data[2], data, TRANSPORT_PAYLOAD_SIZE-1);	// Copy data

		frame.len=PACKET_MAX_DATA;

		offset=TRANSPORT_PAYLOAD_SIZE-1;   // Track data sent

		// Send the frame
		PacketSend(&frame);

		while(offset<len)
		{
			frame.data[0]=0x20|(sequence&0x0F); // 0x20 consecutive frame flag + sequence number

			size_t bytes_to_copy=(len-offset)<TRANSPORT_PAYLOAD_SIZE?(len-offset):TRANSPORT_PAYLOAD_SIZE;
			memcpy(&frame.data[1], data+offset, bytes_to_copy);

			frame.len=bytes_to_copy+1;

			sequence++;
			offset+=bytes_to_copy;

			// Send the frame
			PacketSend(&frame);
		}
	}
}

/// <summary>
/// receiver stuff
/// </summary>
bool PacketReceive(Packet_t *frame)
{
	uint32_t address=0;
	uint16_t port=0;
	int32_t length=Network_SocketReceive(netsocket, frame->data, PACKET_MAX_DATA, &address, &port);

	if(length>0)
	{
		frame->len=length;
		return true;
	}

	return false;
}

bool Transport_Receive(Transport_t *receiver)
{
	double timeout=GetClock()+1.0;

	memset(receiver, 0, sizeof(Transport_t));

	while(1)
	{
		Packet_t frame;

		// TO-DO: Handle timeout here
		if(!PacketReceive(&frame))
		{
			if(GetClock()>timeout)
			{
				printf("Transport_Receive timed out.\n");
				return 0;
			}

			continue;
		}

		// Determine the type of frame
		uint8_t pci_type=(frame.data[0]&0xF0)>>4;

		switch(pci_type)
		{
			case 0x0:	// Single frame
			{
				uint8_t len=frame.data[0]&0x0F;

				memcpy(receiver->buffer, &frame.data[1], len);

				receiver->length=len;
				receiver->offset=len;
				break;
			}

			case 0x1:	// First frame
			{
				size_t len=((frame.data[0]&0x0F)<<8)|frame.data[1];

				memcpy(receiver->buffer, &frame.data[2], TRANSPORT_PAYLOAD_SIZE-1);

				receiver->length=len;
				receiver->offset=TRANSPORT_PAYLOAD_SIZE-1;
				receiver->sequence=1;
				break;
			}

			case 0x2:	// Consecutive frame
			{
				uint8_t seq_num=frame.data[0]&0x0F;

				// Sequence number mismatch
				if(seq_num!=receiver->sequence)
					return 0;

				size_t bytes_to_copy=receiver->length-receiver->offset<TRANSPORT_PAYLOAD_SIZE?receiver->length-receiver->offset:TRANSPORT_PAYLOAD_SIZE;

				memcpy(&receiver->buffer[receiver->offset], &frame.data[1], bytes_to_copy);

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