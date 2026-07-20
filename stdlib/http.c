#include "forge/http.h"
#include "forge/tcp.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    int64_t sock;
    char method[16];
    char path[512];
    char *body;
} fr_http_req_t;

typedef struct {
    int64_t listen_sock;
    int64_t port;
    char *cached_resp;
    size_t cached_len;
} fr_http_server_t;

static fr_http_req_t g_reqs[128];
static fr_http_server_t g_servers[32];

static int recv_until_headers(int fd, char *buf, size_t cap) {
    size_t len = 0;
    while (len + 1 < cap) {
        ssize_t n = recv(fd, buf + len, cap - len - 1, 0);
        if (n < 0) return -1;
        if (n == 0) break;
        len += (size_t)n;
        buf[len] = '\0';
        if (len >= 4 && buf[len - 4] == '\r' && buf[len - 3] == '\n' &&
            buf[len - 2] == '\r' && buf[len - 1] == '\n') {
            break;
        }
    }
    return (int)len;
}

static int tcp_send_all(int fd, const void *data, size_t len) {
    const char *p = (const char *)data;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static void parse_url(const char *url, char *host, size_t hcap, int64_t *port, char *path, size_t pcap) {
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - p);
        if (hlen >= hcap) hlen = hcap - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoll(colon + 1);
        if (slash) snprintf(path, pcap, "%s", slash);
        else snprintf(path, pcap, "/");
    } else {
        size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
        if (hlen >= hcap) hlen = hcap - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = 80;
        if (slash) snprintf(path, pcap, "%s", slash);
        else snprintf(path, pcap, "/");
    }
}

static char *http_exchange(const char *method, const char *url, const char *body) {
    char host[256], path[512];
    int64_t port = 80;
    parse_url(url, host, sizeof(host), &port, path, sizeof(path));

    int64_t sock = fr_tcp_connect(host, port);
    if (sock < 0) return NULL;

    char header[1024];
    if (body && body[0]) {
        snprintf(header, sizeof(header),
                 "%s %s HTTP/1.1\r\nHost: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
                 method, path, host, strlen(body));
    } else {
        snprintf(header, sizeof(header),
                 "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                 method, path, host);
    }
    fr_tcp_send(sock, header);
    if (body && body[0]) fr_tcp_send(sock, body);

    char *resp = fr_tcp_recv(sock);
    fr_tcp_close(sock);
    if (!resp) return NULL;

    char *body_start = strstr(resp, "\r\n\r\n");
    if (!body_start) return resp;
    body_start += 4;
    char *out = (char *)malloc(strlen(body_start) + 1);
    if (!out) { free(resp); return NULL; }
    strcpy(out, body_start);
    free(resp);
    return out;
}

char *fr_http_get(const char *url) {
    return http_exchange("GET", url, NULL);
}

char *fr_http_post(const char *url, const char *body) {
    return http_exchange("POST", url, body ? body : "");
}

int64_t fr_http_listen(int64_t port) {
    int64_t sock = fr_tcp_listen(port);
    if (sock < 0 || sock >= 32) return -1;
    g_servers[sock].listen_sock = sock;
    g_servers[sock].port = port;
    g_servers[sock].cached_resp = NULL;
    g_servers[sock].cached_len = 0;
    return sock;
}

static void parse_http_request(const char *raw, fr_http_req_t *req) {
    req->method[0] = req->path[0] = '\0';
    if (req->body) { free(req->body); req->body = NULL; }

    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return;
    char line[1024];
    size_t ll = (size_t)(line_end - raw);
    if (ll >= sizeof(line)) ll = sizeof(line) - 1;
    memcpy(line, raw, ll);
    line[ll] = '\0';

    sscanf(line, "%15s %511s", req->method, req->path);
    const char *body = strstr(raw, "\r\n\r\n");
    if (body && body[4]) {
        body += 4;
        req->body = (char *)malloc(strlen(body) + 1);
        if (req->body) strcpy(req->body, body);
    }
}

int64_t fr_http_accept(int64_t server) {
    int64_t client = fr_tcp_accept(server);
    if (client < 0 || client >= 128) return -1;

    char buf[4096];
    if (recv_until_headers((int)client, buf, sizeof(buf)) < 0) {
        fr_tcp_close(client);
        return -1;
    }

    g_reqs[client].sock = client;
    parse_http_request(buf, &g_reqs[client]);
    return client;
}

const char *fr_http_req_method(int64_t req) {
    if (req < 0 || req >= 128) return "";
    return g_reqs[req].method;
}

const char *fr_http_req_path(int64_t req) {
    if (req < 0 || req >= 128) return "";
    return g_reqs[req].path;
}

const char *fr_http_req_body(int64_t req) {
    if (req < 0 || req >= 128 || !g_reqs[req].body) return "";
    return g_reqs[req].body;
}

void fr_http_respond(int64_t req, int64_t status, const char *body) {
    if (req < 0 || req >= 128) return;
    const char *text = "OK";
    if (status == 404) text = "Not Found";
    else if (status == 500) text = "Internal Server Error";

    size_t blen = body ? strlen(body) : 0;
    char out[576];
    int hlen = snprintf(out, sizeof(out),
                        "HTTP/1.1 %lld %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
                        (long long)status, text, blen);
    if (hlen < 0 || (size_t)hlen >= sizeof(out)) return;

    if (blen > 0 && (size_t)hlen + blen < sizeof(out)) {
        memcpy(out + hlen, body, blen);
        tcp_send_all((int)g_reqs[req].sock, out, (size_t)hlen + blen);
    } else {
        tcp_send_all((int)g_reqs[req].sock, out, (size_t)hlen);
        if (blen > 0) tcp_send_all((int)g_reqs[req].sock, body, blen);
    }
}

void fr_http_close(int64_t req) {
    if (req < 0 || req >= 128) return;
    if (g_reqs[req].body) { free(g_reqs[req].body); g_reqs[req].body = NULL; }
    fr_tcp_close(g_reqs[req].sock);
    g_reqs[req].sock = -1;
}

void fr_http_server_close(int64_t server) {
    if (server >= 0 && server < 32) {
        free(g_servers[server].cached_resp);
        g_servers[server].cached_resp = NULL;
        g_servers[server].cached_len = 0;
    }
    fr_tcp_close(server);
}

void fr_http_prepare(int64_t server, const char *body) {
    if (server < 0 || server >= 32) return;
    free(g_servers[server].cached_resp);
    g_servers[server].cached_resp = NULL;
    g_servers[server].cached_len = 0;

    size_t blen = body ? strlen(body) : 0;
    size_t cap = 128 + blen;
    char *out = (char *)malloc(cap);
    if (!out) return;
    int n = snprintf(out, cap,
                     "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%.*s",
                     blen, (int)blen, body ? body : "");
    if (n <= 0) { free(out); return; }
    g_servers[server].cached_resp = out;
    g_servers[server].cached_len = (size_t)n;
}

void fr_http_serve_prepared(int64_t server) {
    if (server < 0 || server >= 32) return;
    fr_http_server_t *srv = &g_servers[server];
    if (!srv->cached_resp || srv->cached_len == 0) return;

    int client = (int)fr_tcp_accept(server);
    if (client < 0) return;

    char discard[4096];
    recv(client, discard, sizeof(discard), 0);
    tcp_send_all(client, srv->cached_resp, srv->cached_len);
    close(client);
}

void fr_http_serve_forever(int64_t server) {
    if (server < 0 || server >= 32) return;
    fr_http_server_t *srv = &g_servers[server];
    const char *resp = srv->cached_resp;
    size_t rlen = srv->cached_len;
    if (!resp || rlen == 0) return;

    while (1) {
        int client = (int)accept((int)server, NULL, NULL);
        if (client < 0) continue;
        char discard[4096];
        recv(client, discard, sizeof(discard), 0);
        send(client, resp, rlen, MSG_NOSIGNAL);
        close(client);
    }
}

/* Fast path: read request, respond 200 with fixed body, close — for benchmarks */
void fr_http_serve_ok(int64_t server, const char *body) {
    if (server < 0 || server >= 32) return;
    fr_http_server_t *srv = &g_servers[server];
    if (!srv->cached_resp) fr_http_prepare(server, body);
    fr_http_serve_prepared(server);
}

/* Multi-threaded accept on the same listen socket (benchmark fast path). */
typedef struct {
    int listen_fd;
    const char *resp;
    size_t resp_len;
} http_mt_ctx_t;

static void *http_mt_worker(void *arg) {
    http_mt_ctx_t *ctx = (http_mt_ctx_t *)arg;
    char discard[512];
    while (1) {
        int client = (int)accept(ctx->listen_fd, NULL, NULL);
        if (client < 0) continue;
        recv(client, discard, sizeof(discard), MSG_DONTWAIT);
        send(client, ctx->resp, ctx->resp_len, MSG_NOSIGNAL);
        close(client);
    }
    return NULL;
}

void fr_http_serve_mt(int64_t server, int64_t threads) {
    if (server < 0 || server >= 32) return;
    fr_http_server_t *srv = &g_servers[server];
    if (!srv->cached_resp || srv->cached_len == 0) return;
    int n = (int)(threads > 0 ? threads : 4);
    int listen_fd = (int)server;
    http_mt_ctx_t *ctxs = (http_mt_ctx_t *)calloc((size_t)n, sizeof(http_mt_ctx_t));
    if (!ctxs) return;
    for (int i = 0; i < n; i++) {
        ctxs[i].listen_fd = listen_fd;
        ctxs[i].resp = srv->cached_resp;
        ctxs[i].resp_len = srv->cached_len;
        pthread_t tid;
        pthread_create(&tid, NULL, http_mt_worker, &ctxs[i]);
        pthread_detach(tid);
    }
    while (1) pause();
}
