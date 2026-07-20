#include "forge/tcp.h"
#include "forge/platform.h"
#include <stdlib.h>
#include <string.h>

#if defined(FORGE_OS_WINDOWS)
/* winsock via platform.h */
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

void fr_net_init(void) {
    fr_platform_init();
}

static int set_reuseaddr(fr_socket_t fd) {
    int yes = 1;
#if defined(FORGE_OS_WINDOWS)
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
#else
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
}

int64_t fr_tcp_listen(int64_t port) {
    fr_platform_init();
    fr_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == FR_SOCK_INVALID) return -1;
    set_reuseaddr((int)fd);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { fr_sock_close(fd); return -1; }
    if (listen(fd, 512) < 0) { fr_sock_close(fd); return -1; }
    return (int64_t)fd;
}

int64_t fr_tcp_accept(int64_t sock) {
    if (sock < 0) return -1;
    fr_socket_t client = accept((fr_socket_t)sock, NULL, NULL);
    if (client != FR_SOCK_INVALID) fr_sock_set_tcp_nodelay(client);
    return (int64_t)client;
}

int64_t fr_tcp_connect(const char *host, int64_t port) {
    if (!host) return -1;
    fr_platform_init();
    fr_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == FR_SOCK_INVALID) return -1;
    fr_sock_set_tcp_nodelay(fd);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        fr_sock_close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fr_sock_close(fd);
        return -1;
    }
    return (int64_t)fd;
}

int64_t fr_tcp_send(int64_t sock, const char *data) {
    if (sock < 0 || !data) return -1;
    size_t total = strlen(data);
    ssize_t sent = fr_sock_send(sock, data, total);
    return sent < 0 ? -1 : (int64_t)sent;
}

char *fr_tcp_recv(int64_t sock) {
    if (sock < 0) return NULL;
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nbuf = (char *)realloc(buf, cap);
            if (!nbuf) { free(buf); return NULL; }
            buf = nbuf;
        }
        ssize_t n = fr_sock_recv(sock, buf + len, cap - len - 1);
        if (n < 0) { free(buf); return NULL; }
        if (n == 0) break;
        len += (size_t)n;
    }
    buf[len] = '\0';
    return buf;
}

void fr_tcp_close(int64_t sock) {
    if (sock >= 0) fr_sock_close(sock);
}
