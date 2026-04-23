#ifndef HANDLERS_COMMON_H
#define HANDLERS_COMMON_H

#include <stddef.h>

#include "http.h"
#include "models.h"
#include "string_builder.h"

typedef int (*append_json_item_fn)(StringBuilder *sb, const void *item);

int handlers_write_json_array(
  HttpResponse *response,
  const void *items,
  size_t count,
  size_t item_size,
  append_json_item_fn append_item,
  size_t initial_capacity,
  int status_code
);

int handlers_write_json_item(
  HttpResponse *response,
  const void *item,
  append_json_item_fn append_item,
  size_t initial_capacity,
  int status_code
);

int handlers_append_message_record(const AppConfig *config, const char *text, Message *out_message);

#endif
