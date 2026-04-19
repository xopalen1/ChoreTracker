#include "http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_READ_LIMIT (1024 * 1024)
#define HTTP_CHUNK 4096

static int send_all(socket_t client, const char *data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    int n = send(client, data + sent, (int)(len - sent), 0);
    if (n <= 0) {
      return -1;
    }
    sent += (size_t)n;
  }
  return 0;
}

static int starts_with_ci(const char *line, const char *prefix) {
  while (*prefix && *line) {
    if ((char)tolower((unsigned char)*line) != (char)tolower((unsigned char)*prefix)) {
      return 0;
    }
    ++line;
    ++prefix;
  }
  return *prefix == '\0';
}

static size_t parse_content_length(const char *raw, size_t header_len) {
  size_t i = 0;
  while (i < header_len) {
    const char *line = raw + i;
    const char *line_end = strstr(line, "\r\n");
    if (!line_end) break;
    size_t line_len = (size_t)(line_end - line);
    if (line_len == 0) break;

    if (starts_with_ci(line, "Content-Length:")) {
      const char *value = line + 15;
      while (*value == ' ' || *value == '\t') ++value;
      return (size_t)strtoul(value, NULL, 10);
    }

    i += line_len + 2;
  }
  return 0;
}

int http_read_request(socket_t client, HttpRequest *request) {
  memset(request, 0, sizeof(*request));

  size_t cap = HTTP_CHUNK;
  size_t len = 0;
  char *raw = (char *)malloc(cap + 1);
  if (!raw) return -1;

  char *header_end = NULL;
  while (!header_end) {
    if (len >= HTTP_READ_LIMIT) {
      free(raw);
      return -1;
    }
    if (len + HTTP_CHUNK > cap) {
      cap *= 2;
      char *bigger = (char *)realloc(raw, cap + 1);
      if (!bigger) {
        free(raw);
        return -1;
      }
      raw = bigger;
    }

    int n = recv(client, raw + len, HTTP_CHUNK, 0);
    if (n <= 0) {
      free(raw);
      return -1;
    }
    len += (size_t)n;
    raw[len] = '\0';
    header_end = strstr(raw, "\r\n\r\n");
  }

  size_t header_len = (size_t)(header_end - raw) + 4;
  size_t body_len = parse_content_length(raw, header_len);
  size_t total_len = header_len + body_len;

  while (len < total_len) {
    if (len + HTTP_CHUNK > cap) {
      cap *= 2;
      char *bigger = (char *)realloc(raw, cap + 1);
      if (!bigger) {
        free(raw);
        return -1;
      }
      raw = bigger;
    }

    int n = recv(client, raw + len, HTTP_CHUNK, 0);
    if (n <= 0) {
      free(raw);
      return -1;
    }
    len += (size_t)n;
    raw[len] = '\0';
  }

  char *line_end = strstr(raw, "\r\n");
  if (!line_end) {
    free(raw);
    return -1;
  }

  *line_end = '\0';
  if (sscanf(raw, "%7s %255s", request->method, request->path) != 2) {
    free(raw);
    return -1;
  }

  request->body_len = body_len;
  request->body = (char *)malloc(body_len + 1);
  if (!request->body) {
    free(raw);
    return -1;
  }

  memcpy(request->body, raw + header_len, body_len);
  request->body[body_len] = '\0';

  free(raw);
  return 0;
}

void http_request_free(HttpRequest *request) {
  if (request && request->body) {
    free(request->body);
    request->body = NULL;
    request->body_len = 0;
  }
}

void http_response_set(HttpResponse *response, int status, const char *content_type, const char *body) {
  response->status = status;
  response->content_type = content_type;
  size_t len = body ? strlen(body) : 0;
  response->body = (char *)malloc(len + 1);
  if (!response->body) {
    return;
  }
  if (body) {
    memcpy(response->body, body, len);
  }
  response->body[len] = '\0';
}

void http_response_set_json(HttpResponse *response, int status, const char *json_body) {
  http_response_set(response, status, "application/json", json_body);
}

void http_response_set_error(HttpResponse *response, int status, const char *message) {
  char body[512];
  snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message ? message : "Request failed");
  http_response_set_json(response, status, body);
}

void http_response_free(HttpResponse *response) {
  if (response && response->body) {
    free(response->body);
    response->body = NULL;
  }
}

static const char *status_text(int status) {
  switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return "OK";
  }
}

int http_send_response(socket_t client, const HttpResponse *response) {
  const char *body = response->body ? response->body : "";
  size_t body_len = strlen(body);

  char header[512];
  int header_len = snprintf(
    header,
    sizeof(header),
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %zu\r\n"
    "Connection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Access-Control-Allow-Methods: GET,POST,PATCH,OPTIONS\r\n"
    "\r\n",
    response->status,
    status_text(response->status),
    response->content_type ? response->content_type : "application/json",
    body_len
  );

  if (header_len <= 0) {
    return -1;
  }

  if (send_all(client, header, (size_t)header_len) != 0) {
    return -1;
  }

  if (body_len > 0 && send_all(client, body, body_len) != 0) {
    return -1;
  }

  return 0;
}
