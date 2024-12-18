#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdint.h>
#include <stdbool.h>

typedef int Socket_t;

#define NETWORK_ADDRESS(a, b, c, d) ((a<<24)|(b<<16)|(c<<8)|d)

bool Network_Init(void);
void Network_Destroy(void);
Socket_t Network_CreateSocket(void);
bool Network_SocketBind(const Socket_t sock, const uint32_t address, const uint16_t port);
bool Network_SocketSend(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, const uint32_t address, const uint16_t port);
int32_t Network_SocketReceive(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, uint32_t *address, uint16_t *port);
bool Network_SocketClose(const Socket_t sock);
int32_t ReliableSocketReceive(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, uint32_t *address, uint16_t *port, const double timeout);
bool ReliableSocketSend(const Socket_t sock, const uint8_t *buffer, const uint32_t buffer_size, const uint32_t address, const uint16_t port, const double timeout);

#endif
