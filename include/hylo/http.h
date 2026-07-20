#ifndef HYLO_HTTP_H
#define HYLO_HTTP_H

#include <stdint.h>

char *hy_http_get(const char *url);
char *hy_http_post(const char *url, const char *body);
int64_t hy_http_listen(int64_t port);
int64_t hy_http_accept(int64_t server);
const char *hy_http_req_method(int64_t req);
const char *hy_http_req_path(int64_t req);
const char *hy_http_req_body(int64_t req);
void hy_http_respond(int64_t req, int64_t status, const char *body);
void hy_http_close(int64_t req);
void hy_http_server_close(int64_t server);

#endif
