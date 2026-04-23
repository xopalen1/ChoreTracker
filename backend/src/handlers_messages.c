#include "handlers.h"

#include <stdio.h>

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

  Message created;
  if (handlers_append_message_record(config, text, &created) != 0) {
    http_response_set_error(response, 500, "Could not write messages CSV");
    return;
  }

  handlers_write_json_item(response, &created, append_message_json, 256, 201);
}
