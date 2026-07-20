#ifndef HYLO_UDP_H
#define HYLO_UDP_H

#include <stdint.h>

int64_t hy_udp_bind(int64_t port);
int64_t hy_udp_send(int64_t sock, const char *host, int64_t port, const char *data);
char *hy_udp_recv(int64_t sock);
const char *hy_udp_peer(int64_t sock);
void hy_udp_close(int64_t sock);

#endif
