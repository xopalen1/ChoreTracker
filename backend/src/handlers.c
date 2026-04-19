#include "handlers.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "date_utils.h"
#include "db.h"
#include "json_utils.h"

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} StringBuilder;

static int sb_init(StringBuilder *sb, size_t initial_cap) {
  sb->data = (char *)malloc(initial_cap);
  if (!sb->data) return -1;
  sb->data[0] = '\0';
  sb->len = 0;
  sb->cap = initial_cap;
  return 0;
}

static int sb_reserve(StringBuilder *sb, size_t needed) {
  if (sb->len + needed + 1 <= sb->cap) return 0;

  size_t next_cap = sb->cap;
  while (sb->len + needed + 1 > next_cap) {
    next_cap *= 2;
  }

  char *bigger = (char *)realloc(sb->data, next_cap);
  if (!bigger) return -1;
  sb->data = bigger;
  sb->cap = next_cap;
  return 0;
}

static int sb_append(StringBuilder *sb, const char *text) {
  size_t n = strlen(text);
  if (sb_reserve(sb, n) != 0) return -1;
  memcpy(sb->data + sb->len, text, n);
  sb->len += n;
  sb->data[sb->len] = '\0';
  return 0;
}

static int sb_appendf(StringBuilder *sb, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_list args_copy;
  va_copy(args_copy, args);

  int needed = vsnprintf(NULL, 0, fmt, args_copy);
  va_end(args_copy);

  if (needed < 0) {
    va_end(args);
    return -1;
  }

  if (sb_reserve(sb, (size_t)needed) != 0) {
    va_end(args);
    return -1;
  }

  vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, args);
  sb->len += (size_t)needed;
  va_end(args);
  return 0;
}

static void sb_free(StringBuilder *sb) {
  free(sb->data);
  sb->data = NULL;
  sb->len = 0;
  sb->cap = 0;
}

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

static int append_message_json(StringBuilder *sb, const Message *message) {
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

void handle_get_messages(const AppConfig *config, HttpResponse *response) {
  MessageList list;
  if (db_load_messages(config->messages_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read messages CSV");
    return;
  }

  StringBuilder sb;
  if (sb_init(&sb, 512) != 0) {
    db_free_messages(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }

  sb_append(&sb, "[");
  for (size_t i = 0; i < list.count; ++i) {
    if (i > 0) sb_append(&sb, ",");
    append_message_json(&sb, &list.items[i]);
  }
  sb_append(&sb, "]");

  http_response_set_json(response, 200, sb.data);

  sb_free(&sb);
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

  StringBuilder sb;
  if (sb_init(&sb, 256) != 0) {
    db_free_messages(&list);
    http_response_set_error(response, 500, "Out of memory");
    return;
  }

  append_message_json(&sb, message);
  http_response_set_json(response, 201, sb.data);

  sb_free(&sb);
  db_free_messages(&list);
}
