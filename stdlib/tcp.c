#include "forge/tcp.h"
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void fr_net_init(void) {}

static int set_reuseaddr(int fd) {
    int yes = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

int64_t fr_tcp_listen(int64_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    set_reuseaddr(fd);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 512) < 0) { close(fd); return -1; }
    return fd;
}

static void tcp_tune_client(int fd) {
    int yes = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
}

int64_t fr_tcp_accept(int64_t sock) {
    if (sock < 0) return -1;
    int client = accept((int)sock, NULL, NULL);
    if (client >= 0) tcp_tune_client(client);
    return client;
}

int64_t fr_tcp_connect(const char *host, int64_t port) {
    if (!host) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    tcp_tune_client(fd);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int64_t fr_tcp_send(int64_t sock, const char *data) {
    if (sock < 0 || !data) return -1;
    size_t total = strlen(data);
    size_t sent = 0;
    while (sent < total) {
        ssize_t n = send((int)sock, data + sent, total - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return (int64_t)sent;
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
        ssize_t n = recv((int)sock, buf + len, cap - len - 1, 0);
        if (n < 0) { free(buf); return NULL; }
        if (n == 0) break;
        len += (size_t)n;
    }
    buf[len] = '\0';
    return buf;
}

void fr_tcp_close(int64_t sock) {
    if (sock >= 0) close((int)sock);
}
