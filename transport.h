#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

bool Transport_Send(const Socket_t sock, const uint8_t *data, const size_t len, const uint32_t address, const uint16_t port, const double timeout);
uint8_t *Transport_Receive(const Socket_t sock, size_t *length, uint32_t *address, uint16_t *port, const double timeout);

#endif
