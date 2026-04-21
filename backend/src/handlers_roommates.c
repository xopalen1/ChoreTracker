#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "handlers_common.h"
#include "json_utils.h"
#include "string_builder.h"
#include "text_utils.h"

static int append_roommate_json(StringBuilder *sb, const void *item) {
  const Roommate *roommate = (const Roommate *)item;
  char id[ROOMMATE_ID_MAX * 2];
  char name[ROOMMATE_NAME_MAX * 2];

  json_escape(roommate->id, id, sizeof(id));
  json_escape(roommate->name, name, sizeof(name));

  return sb_appendf(sb, "{\"id\":\"%s\",\"name\":\"%s\"}", id, name);
}

void handle_get_roommates(const AppConfig *config, HttpResponse *response) {
  RoommateList list;
  if (db_load_roommates(config->roommates_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read roommates CSV");
    return;
  }

  handlers_write_json_array(response, list.items, list.count, sizeof(Roommate), append_roommate_json, 256, 200);
  db_free_roommates(&list);
}

void handle_create_roommate(const AppConfig *config, const HttpRequest *request, HttpResponse *response) {
  char name[ROOMMATE_NAME_MAX];

  if (!json_get_string(request->body, "name", name, sizeof(name))) {
    http_response_set_error(response, 400, "Missing required field: name");
    return;
  }

  trim_whitespace_in_place(name);
  if (name[0] == '\0') {
    http_response_set_error(response, 400, "Roommate name must not be empty");
    return;
  }

  RoommateList list;
  if (db_load_roommates(config->roommates_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read roommates CSV");
    return;
  }

  if (db_roommate_exists(&list, name)) {
    db_free_roommates(&list);
    http_response_set_error(response, 409, "Roommate already exists");
    return;
  }

  Roommate *bigger = (Roommate *)realloc(list.items, (list.count + 1) * sizeof(Roommate));
  if (!bigger) {
    db_free_roommates(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }
  list.items = bigger;

  Roommate *roommate = &list.items[list.count];
  memset(roommate, 0, sizeof(*roommate));
  db_next_roommate_id(&list, roommate->id, sizeof(roommate->id));
  snprintf(roommate->name, sizeof(roommate->name), "%s", name);
  list.count += 1;

  if (db_save_roommates(config->roommates_csv_path, &list) != 0) {
    db_free_roommates(&list);
    http_response_set_error(response, 500, "Could not write roommates CSV");
    return;
  }

  handlers_write_json_item(response, roommate, append_roommate_json, 128, 201);
  db_free_roommates(&list);
}
