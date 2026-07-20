#include "forge/http.h"
#include "forge/tcp.h"
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
} fr_http_server_t;

static fr_http_req_t g_reqs[128];
static fr_http_server_t g_servers[32];

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
    if (body) {
        body += 4;
        req->body = (char *)malloc(strlen(body) + 1);
        if (req->body) strcpy(req->body, body);
    }
}

static char *tcp_recv_until_headers(int64_t sock) {
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
        buf[len] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }
    buf[len] = '\0';
    return buf;
}

int64_t fr_http_accept(int64_t server) {
    int64_t client = fr_tcp_accept(server);
    if (client < 0 || client >= 128) return -1;
    char *raw = tcp_recv_until_headers(client);
    g_reqs[client].sock = client;
    parse_http_request(raw ? raw : "", &g_reqs[client]);
    free(raw);
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
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %lld %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
             (long long)status, text, body ? strlen(body) : 0);
    fr_tcp_send(g_reqs[req].sock, header);
    if (body && body[0]) fr_tcp_send(g_reqs[req].sock, body);
}

void fr_http_close(int64_t req) {
    if (req < 0 || req >= 128) return;
    if (g_reqs[req].body) { free(g_reqs[req].body); g_reqs[req].body = NULL; }
    fr_tcp_close(g_reqs[req].sock);
    g_reqs[req].sock = -1;
}

void fr_http_server_close(int64_t server) {
    fr_tcp_close(server);
}
