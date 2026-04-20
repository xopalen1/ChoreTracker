#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "date_utils.h"
#include "db.h"
#include "json_utils.h"
#include "string_builder.h"
#include "text_utils.h"

static const char *chore_status(const Chore *chore) {
  if (chore->is_deleted) return "deleted";
  if (chore->is_done) return "done";
  return "open";
}

static int append_chore_json(StringBuilder *sb, const Chore *chore) {
  char id[CHORE_ID_MAX * 2];
  char title[CHORE_TITLE_MAX * 2];
  char assignee[CHORE_ASSIGNEE_MAX * 2];
  char assigned[DATE_MAX * 2];
  char due[DATE_MAX * 2];

  json_escape(chore->id, id, sizeof(id));
  json_escape(chore->title, title, sizeof(title));
  json_escape(chore->assignee, assignee, sizeof(assignee));
  json_escape(chore->assigned_date, assigned, sizeof(assigned));
  json_escape(chore->due_date, due, sizeof(due));

  const char *history = chore->due_date_history[0] ? chore->due_date_history : "[]";

  return sb_appendf(
    sb,
    "{\"id\":\"%s\",\"title\":\"%s\",\"assignee\":\"%s\"," 
    "\"assignedDate\":\"%s\",\"dueDate\":\"%s\"," 
    "\"isDone\":%s,\"isDeleted\":%s,\"dueDateHistory\":%s,\"status\":\"%s\"}",
    id,
    title,
    assignee,
    assigned,
    due,
    chore->is_done ? "true" : "false",
    chore->is_deleted ? "true" : "false",
    history,
    chore_status(chore)
  );
}

void handle_get_chores(const AppConfig *config, HttpResponse *response) {
  ChoreList list;
  if (db_load_chores(config->chores_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read chores CSV");
    return;
  }

  StringBuilder sb;
  if (sb_init(&sb, 1024) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }

  sb_append(&sb, "[");
  for (size_t i = 0; i < list.count; ++i) {
    if (i > 0) sb_append(&sb, ",");
    append_chore_json(&sb, &list.items[i]);
  }
  sb_append(&sb, "]");

  http_response_set_json(response, 200, sb.data);

  sb_free(&sb);
  db_free_chores(&list);
}

void handle_get_chore_by_id(const AppConfig *config, const char *id, HttpResponse *response) {
  ChoreList list;
  if (db_load_chores(config->chores_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read chores CSV");
    return;
  }

  Chore *found = NULL;
  for (size_t i = 0; i < list.count; ++i) {
    if (strcmp(list.items[i].id, id) == 0) {
      found = &list.items[i];
      break;
    }
  }

  if (!found) {
    db_free_chores(&list);
    http_response_set_error(response, 404, "Chore not found");
    return;
  }

  StringBuilder sb;
  if (sb_init(&sb, 512) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }

  append_chore_json(&sb, found);
  http_response_set_json(response, 200, sb.data);

  sb_free(&sb);
  db_free_chores(&list);
}

void handle_create_chore(const AppConfig *config, const HttpRequest *request, HttpResponse *response) {
  char title[CHORE_TITLE_MAX];
  char assignee[CHORE_ASSIGNEE_MAX];
  char due_date[DATE_MAX];

  if (!json_get_string(request->body, "title", title, sizeof(title)) ||
      !json_get_string(request->body, "assignee", assignee, sizeof(assignee)) ||
      !json_get_string(request->body, "dueDate", due_date, sizeof(due_date))) {
    http_response_set_error(response, 400, "Missing required fields: title, assignee, dueDate");
    return;
  }

  trim_whitespace_in_place(title);
  trim_whitespace_in_place(assignee);

  if (title[0] == '\0' || assignee[0] == '\0') {
    http_response_set_error(response, 400, "Title and assignee must not be empty");
    return;
  }

  RoommateList roommates;
  if (db_load_roommates(config->roommates_csv_path, &roommates) != 0) {
    http_response_set_error(response, 500, "Could not read roommates CSV");
    return;
  }

  if (!db_roommate_exists(&roommates, assignee)) {
    db_free_roommates(&roommates);
    http_response_set_error(response, 400, "Assignee must be an existing roommate");
    return;
  }
  db_free_roommates(&roommates);

  ChoreList list;
  if (db_load_chores(config->chores_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read chores CSV");
    return;
  }

  Chore *bigger = (Chore *)realloc(list.items, (list.count + 1) * sizeof(Chore));
  if (!bigger) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }
  list.items = bigger;

  Chore *chore = &list.items[list.count];
  memset(chore, 0, sizeof(*chore));

  db_next_chore_id(&list, chore->id, sizeof(chore->id));
  snprintf(chore->title, sizeof(chore->title), "%s", title);
  snprintf(chore->assignee, sizeof(chore->assignee), "%s", assignee);

  // New chores are always assigned on Sunday by backend rule.
  date_next_sunday_iso(chore->assigned_date, sizeof(chore->assigned_date));
  snprintf(chore->due_date, sizeof(chore->due_date), "%s", due_date);

  chore->is_done = false;
  chore->is_deleted = false;
  snprintf(chore->due_date_history, sizeof(chore->due_date_history), "%s", "[]");

  list.count += 1;

  if (db_save_chores(config->chores_csv_path, &list) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Could not write chores CSV");
    return;
  }

  StringBuilder sb;
  if (sb_init(&sb, 512) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }

  append_chore_json(&sb, chore);
  http_response_set_json(response, 201, sb.data);

  sb_free(&sb);
  db_free_chores(&list);
}

void handle_patch_chore(const AppConfig *config, const char *id, const HttpRequest *request, HttpResponse *response) {
  ChoreList list;
  if (db_load_chores(config->chores_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read chores CSV");
    return;
  }

  Chore *found = NULL;
  for (size_t i = 0; i < list.count; ++i) {
    if (strcmp(list.items[i].id, id) == 0) {
      found = &list.items[i];
      break;
    }
  }

  if (!found) {
    db_free_chores(&list);
    http_response_set_error(response, 404, "Chore not found");
    return;
  }

  int changed = 0;
  bool bool_value = false;
  char due_date[DATE_MAX];
  char history[HISTORY_MAX];

  if (json_get_bool(request->body, "isDone", &bool_value)) {
    found->is_done = bool_value;
    changed = 1;
  }

  if (json_get_bool(request->body, "isDeleted", &bool_value)) {
    found->is_deleted = bool_value;
    changed = 1;
  }

  if (json_get_string(request->body, "dueDate", due_date, sizeof(due_date))) {
    snprintf(found->due_date, sizeof(found->due_date), "%s", due_date);
    changed = 1;
  }

  if (json_get_array_raw(request->body, "dueDateHistory", history, sizeof(history))) {
    snprintf(found->due_date_history, sizeof(found->due_date_history), "%s", history);
    changed = 1;
  }

  if (!changed) {
    db_free_chores(&list);
    http_response_set_error(response, 400, "No supported patch fields found");
    return;
  }

  if (db_save_chores(config->chores_csv_path, &list) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Could not write chores CSV");
    return;
  }

  StringBuilder sb;
  if (sb_init(&sb, 512) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }

  append_chore_json(&sb, found);
  http_response_set_json(response, 200, sb.data);

  sb_free(&sb);
  db_free_chores(&list);
}
