#include "handlers_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "date_utils.h"
#include "db.h"

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

int handlers_append_message_record(const AppConfig *config, const char *text, Message *out_message) {
  if (!config || !text) {
    return -1;
  }

  MessageList messages;
  if (db_load_messages(config->messages_csv_path, &messages) != 0) {
    return -1;
  }

  Message *bigger = (Message *)realloc(messages.items, (messages.count + 1) * sizeof(Message));
  if (!bigger) {
    db_free_messages(&messages);
    return -1;
  }
  messages.items = bigger;

  Message *message = &messages.items[messages.count];
  memset(message, 0, sizeof(*message));

  if (db_next_message_id(&messages, message->id, sizeof(message->id)) != 0) {
    db_free_messages(&messages);
    return -1;
  }

  snprintf(message->text, sizeof(message->text), "%s", text);
  date_now_iso_datetime(message->sent_at, sizeof(message->sent_at));
  messages.count += 1;

  if (db_save_messages(config->messages_csv_path, &messages) != 0) {
    db_free_messages(&messages);
    return -1;
  }

  if (out_message) {
    *out_message = *message;
  }

  db_free_messages(&messages);
  return 0;
}
