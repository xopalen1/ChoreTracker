#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "date_utils.h"
#include "db.h"
#include "handlers_common.h"
#include "json_utils.h"
#include "string_builder.h"

static int append_message_json(StringBuilder *sb, const void *item) {
  const Message *message = (const Message *)item;
  char id[MESSAGE_ID_MAX * 2];
  char text[MESSAGE_TEXT_MAX * 2];
  char sent_at[DATETIME_MAX * 2];

  json_escape(message->id, id, sizeof(id));
  json_escape(message->text, text, sizeof(text));
  json_escape(message->sent_at, sent_at, sizeof(sent_at));

  return sb_appendf(
    sb,
    "{\"id\":\"%s\",\"text\":\"%s\",\"sentAt\":\"%s\"}",
    id,
    text,
    sent_at
  );
}

void handle_get_messages(const AppConfig *config, HttpResponse *response) {
  MessageList list;
  if (db_load_messages(config->messages_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read messages CSV");
    return;
  }

  handlers_write_json_array(response, list.items, list.count, sizeof(Message), append_message_json, 512, 200);
  db_free_messages(&list);
}

void handle_create_message(const AppConfig *config, const HttpRequest *request, HttpResponse *response) {
  char text[MESSAGE_TEXT_MAX];

  if (!json_get_string(request->body, "text", text, sizeof(text))) {
    http_response_set_error(response, 400, "Missing required field: text");
    return;
  }

  MessageList list;
  if (db_load_messages(config->messages_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read messages CSV");
    return;
  }

  Message *bigger = (Message *)realloc(list.items, (list.count + 1) * sizeof(Message));
  if (!bigger) {
    db_free_messages(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }
  list.items = bigger;

  Message *message = &list.items[list.count];
  memset(message, 0, sizeof(*message));

  db_next_message_id(&list, message->id, sizeof(message->id));
  snprintf(message->text, sizeof(message->text), "%s", text);
  date_now_iso_datetime(message->sent_at, sizeof(message->sent_at));

  list.count += 1;

  if (db_save_messages(config->messages_csv_path, &list) != 0) {
    db_free_messages(&list);
    http_response_set_error(response, 500, "Could not write messages CSV");
    return;
  }

  handlers_write_json_item(response, message, append_message_json, 256, 201);
  db_free_messages(&list);
}
