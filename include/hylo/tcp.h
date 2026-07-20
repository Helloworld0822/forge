#ifndef HYLO_TCP_H
#define HYLO_TCP_H

#include <stdint.h>

int64_t hy_tcp_listen(int64_t port);
int64_t hy_tcp_accept(int64_t sock);
int64_t hy_tcp_connect(const char *host, int64_t port);
int64_t hy_tcp_send(int64_t sock, const char *data);
char *hy_tcp_recv(int64_t sock);
void hy_tcp_close(int64_t sock);
void hy_net_init(void);

#endif
