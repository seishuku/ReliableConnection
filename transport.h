#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

#define PACKET_MAX_DATA 8
#define TRANSPORT_PAYLOAD_SIZE (PACKET_MAX_DATA-1)

typedef struct
{
	uint8_t data[PACKET_MAX_DATA];
	uint8_t len;
} Packet_t;

typedef struct
{
	uint8_t buffer[4096];
	size_t length;
	size_t offset;
	uint8_t sequence;
} Transport_t;

void Transport_Send(const uint8_t *data, size_t len);
bool Transport_Receive(Transport_t *receiver);

#endif
