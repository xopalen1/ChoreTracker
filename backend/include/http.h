#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include "net.h"

typedef struct {
  char method[8];
  char path[256];
  char *body;
  size_t body_len;
} HttpRequest;

typedef struct {
  int status;
  const char *content_type;
  char *body;
} HttpResponse;

int http_read_request(socket_t client, HttpRequest *request);
void http_request_free(HttpRequest *request);

void http_response_set(HttpResponse *response, int status, const char *content_type, const char *body);
void http_response_set_json(HttpResponse *response, int status, const char *json_body);
void http_response_set_error(HttpResponse *response, int status, const char *message);
void http_response_free(HttpResponse *response);

int http_send_response(socket_t client, const HttpResponse *response);

#endif
