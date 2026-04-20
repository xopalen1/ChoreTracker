#include "router.h"

#include <string.h>

#include "handlers.h"

static const char *path_param(const char *path, const char *prefix) {
  size_t prefix_len = strlen(prefix);
  if (strncmp(path, prefix, prefix_len) != 0) {
    return NULL;
  }
  const char *value = path + prefix_len;
  if (*value == '\0') {
    return NULL;
  }
  if (strchr(value, '/')) {
    return NULL;
  }
  return value;
}

void router_handle_request(const AppConfig *config, const HttpRequest *request, HttpResponse *response) {
  if (strcmp(request->method, "OPTIONS") == 0) {
    http_response_set_json(response, 200, "{}");
    return;
  }

  if (strcmp(request->method, "GET") == 0 && strcmp(request->path, "/api/chores") == 0) {
    handle_get_chores(config, response);
    return;
  }

  if (strcmp(request->method, "POST") == 0 && strcmp(request->path, "/api/chores") == 0) {
    handle_create_chore(config, request, response);
    return;
  }

  const char *chore_id = path_param(request->path, "/api/chores/");
  if (chore_id && strcmp(request->method, "GET") == 0) {
    handle_get_chore_by_id(config, chore_id, response);
    return;
  }

  if (chore_id && strcmp(request->method, "PATCH") == 0) {
    handle_patch_chore(config, chore_id, request, response);
    return;
  }

  if (strcmp(request->method, "GET") == 0 && strcmp(request->path, "/api/messages") == 0) {
    handle_get_messages(config, response);
    return;
  }

  if (strcmp(request->method, "POST") == 0 && strcmp(request->path, "/api/messages") == 0) {
    handle_create_message(config, request, response);
    return;
  }

  if (strcmp(request->method, "GET") == 0 && strcmp(request->path, "/api/roommates") == 0) {
    handle_get_roommates(config, response);
    return;
  }

  if (strcmp(request->method, "POST") == 0 && strcmp(request->path, "/api/roommates") == 0) {
    handle_create_roommate(config, request, response);
    return;
  }

  http_response_set_error(response, 404, "Route not found");
}
