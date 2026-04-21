#include "handlers_common.h"

int handlers_write_json_array(
  HttpResponse *response,
  const void *items,
  size_t count,
  size_t item_size,
  append_json_item_fn append_item,
  size_t initial_capacity,
  int status_code
) {
  if (!response || !append_item || (count > 0 && (!items || item_size == 0))) {
    return -1;
  }

  StringBuilder sb;
  if (sb_init(&sb, initial_capacity) != 0) {
    http_response_set_error(response, 500, "Out of memory");
    return -1;
  }

  if (sb_append(&sb, "[") != 0) {
    sb_free(&sb);
    http_response_set_error(response, 500, "Out of memory");
    return -1;
  }

  const char *cursor = (const char *)items;
  for (size_t i = 0; i < count; ++i) {
    if (i > 0 && sb_append(&sb, ",") != 0) {
      sb_free(&sb);
      http_response_set_error(response, 500, "Out of memory");
      return -1;
    }

    const void *item = (const void *)(cursor + (i * item_size));
    if (append_item(&sb, item) != 0) {
      sb_free(&sb);
      http_response_set_error(response, 500, "Out of memory");
      return -1;
    }
  }

  if (sb_append(&sb, "]") != 0) {
    sb_free(&sb);
    http_response_set_error(response, 500, "Out of memory");
    return -1;
  }

  http_response_set_json(response, status_code, sb.data);
  sb_free(&sb);
  return 0;
}

int handlers_write_json_item(
  HttpResponse *response,
  const void *item,
  append_json_item_fn append_item,
  size_t initial_capacity,
  int status_code
) {
  if (!response || !append_item || !item) {
    return -1;
  }

  StringBuilder sb;
  if (sb_init(&sb, initial_capacity) != 0) {
    http_response_set_error(response, 500, "Out of memory");
    return -1;
  }

  if (append_item(&sb, item) != 0) {
    sb_free(&sb);
    http_response_set_error(response, 500, "Out of memory");
    return -1;
  }

  http_response_set_json(response, status_code, sb.data);
  sb_free(&sb);
  return 0;
}
