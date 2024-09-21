/*

One-shot HTTP server to receive and parse OAuth codes (the final step of PKCE).

Includes utilities to parse HTTP/1.1 URLs and query strings to perform some
basic routing.

Limitations:
 - Single-threaded
 - Each connection is closed after the first request/response (no socket reuse).


*/

#ifndef __SNP_HTTP_SERVER_H__
#define __SNP_HTTP_SERVER_H__

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  HTTP_METHOD_GET = 0,
  HTTP_METHOD_HEAD,
  HTTP_METHOD_POST,
  HTTP_METHOD_PUT,
  HTTP_METHOD_DELETE,
  HTTP_METHOD_CONNECT,
  HTTP_METHOD_OPTIONS,
  HTTP_METHOD_TRACE,
  HTTP_METHOD_PATCH,
  HTTP_METHOD_UNKNOWN
} HttpMethod;

typedef struct HttpRequestQueryParam HttpRequestQueryParam;

typedef struct {
  HttpMethod method;
  char path[1024];
  char __query[1024];
  HttpRequestQueryParam *query;
} HttpRequest;

/**
 * Assumption: request is a valid HTTP/1.1 request message
 */
HttpRequest *http_request_parse_message(char *request);
void http_request_free(HttpRequest *req);
const char *http_request_query_get(HttpRequest *request, const char *key);
void http_request_print(HttpRequest *request);

typedef struct {
  unsigned short code;
  char content_type[64];
  char body[512];
} HttpResponse;

typedef struct {
  int socket;
  struct sockaddr_in address;
} HttpServer;

// void http_server_init(HttpServer *server, unsigned short port);
// void http_server_destroy(HttpServer *server);
/**
 * Run server until `callback` returns non-zero.
 */
int http_server_run_until(unsigned short port,
                          int (*callback)(HttpRequest *request,
                                          HttpResponse *response,
                                          void *user_data),
                          void *user_data);

#endif // __SNP_HTTP_SERVER_H__
