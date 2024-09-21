#include "http-server.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct HttpRequestQueryParam {
  const char *key;
  const char *value;
  struct HttpRequestQueryParam *next;
} HttpRequestQueryParam;

static inline const char *http_method_to_string(HttpMethod method) {
  static const char *strings[] = {
      "GET",     "HEAD",    "POST",  "PUT",   "DELETE",
      "CONNECT", "OPTIONS", "TRACE", "PATCH", "UNKNOWN",
  };
  return strings[method];
}

static inline HttpMethod http_method_from_string(const char *string) {
  switch (string[0]) {
  case 'C':
    return HTTP_METHOD_CONNECT;
  case 'D':
    return HTTP_METHOD_DELETE;
  case 'G':
    return HTTP_METHOD_GET;
  case 'H':
    return HTTP_METHOD_HEAD;
  case 'O':
    return HTTP_METHOD_OPTIONS;
  case 'P':
    switch (string[1]) {
    case 'O':
      return HTTP_METHOD_POST;
    case 'U':
      return HTTP_METHOD_PUT;
    case 'A':
      return HTTP_METHOD_PATCH;
    }
    break;
  case 'T':
    return HTTP_METHOD_TRACE;
  }
  return HTTP_METHOD_UNKNOWN;
}

/** WARNING: MODIFIES PROVIDED BUFFER */
HttpRequest *http_request_parse_message(char *message) {
  HttpRequest *out = malloc(sizeof(HttpRequest));
  char *first_line = strtok(message, "\r\n");

  char *method = strtok(first_line, " ");
  char *location = strtok(NULL, " ");

  out->method = http_method_from_string(method);

  char *path = strtok(location, "?");
  strcpy(out->path, path);

  char *query = strtok(NULL, "");
  if (query) {
    strcpy(out->__query, query);
    query = out->__query;

    HttpRequestQueryParam head;
    HttpRequestQueryParam *prev = &head;
    HttpRequestQueryParam *new;

    for (char *segment = strtok(query, "&"); segment != NULL;
         segment = strtok(NULL, "&")) {
      new = malloc(sizeof(HttpRequestQueryParam));
      char *key = segment;
      new->key = key;
      char *value = strchr(key, '=');
      if (value != NULL) {
        value[0] = 0;
        value += 1;
        new->value = value;
      } else {
        new->value = key + sizeof(key);
      }
      prev->next = new;
      prev = new;
    }
    out->query = head.next;
  } else {
    out->query = NULL;
  }
  return out;
}

void http_request_free(HttpRequest *req) {
  if (req == NULL)
    return;

  HttpRequestQueryParam *curr = req->query;
  HttpRequestQueryParam *next;
  while (curr) {
    next = curr->next;
    free(curr);
    curr = next;
  }

  free(req);
}

const char *http_request_query_get(HttpRequest *request, const char *key) {
  HttpRequestQueryParam *curr = request->query;
  while (curr) {
    if (strcmp(curr->key, key) == 0) {
      return curr->value;
    }
    curr = curr->next;
  }
  return NULL;
}

void http_request_print(HttpRequest *request) {
  printf("method: %s\n"
         "path: %s\n",
         // "query: %s\n",
         http_method_to_string(request->method), request->path);
}

int http_server_run_until(unsigned short port,
                          int (*callback)(HttpRequest *, HttpResponse *,
                                          void *),
                          void *user_data) {
  int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == -1) {
    perror("socket");
    return EXIT_FAILURE;
  }

  int enable = 1;
  if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) <
      0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    goto socket_failure;
  }

  struct sockaddr_in address = {.sin_family = AF_INET,
                                .sin_addr = {.s_addr = htonl(INADDR_ANY)},
                                .sin_port = htons(port)};
  if (bind(server, (struct sockaddr *)&address, sizeof(address)) == -1) {
    perror("bind");
    goto socket_failure;
  }

  if (listen(server, 5) == -1) {
    perror("listen");
    goto socket_failure;
  }

  struct sockaddr_in client_address;
  socklen_t client_len;
  int client_socket;
  int n_bytes;
  char buf[BUFSIZ];
  int should_exit = 0;

  HttpResponse response = {0};
  char serialized_response[BUFSIZ];

  while (!should_exit) {
    client_len = sizeof(struct sockaddr_in);
    client_socket =
        accept(server, (struct sockaddr *)&client_address, &client_len);
    if (client_socket < 0) {
      perror("accept");
      goto socket_failure;
    }

    n_bytes = read(client_socket, buf, BUFSIZ - 1);
    if (n_bytes > 0) {
      buf[n_bytes] = 0; // terminate with null

      // reset response data
      strcpy(response.content_type, "text/html");
      response.code = 200;
      response.body[0] = 0;

      // parse request
      HttpRequest *request = http_request_parse_message(buf);

      should_exit = callback(request, &response, user_data);
      http_request_free(request);

      sprintf(serialized_response,
              "HTTP/1.1 %d\r\n"
              "Content-Type: %s\r\n"
              "\r\n"
              "%s",
              response.code, response.content_type, response.body);
      write(client_socket, serialized_response,
            strlen(serialized_response) + 1);
    }
    close(client_socket);
  }
  close(server);
  return EXIT_SUCCESS;

socket_failure:
  close(server);
  return EXIT_FAILURE;
}
