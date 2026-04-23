#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "date_utils.h"
#include "db.h"
#include "json_utils.h"
#include "handlers_common.h"
#include "reassignment.h"
#include "string_builder.h"
#include "text_utils.h"

static const char *chore_status(const Chore *chore) {
  if (chore->is_done) return "done";
  return "open";
}

static int find_chore_index(const ChoreList *list, const char *id, size_t *out_index) {
  if (!list || !id || !out_index) return -1;

  for (size_t i = 0; i < list->count; ++i) {
    if (strcmp(list->items[i].id, id) == 0) {
      *out_index = i;
      return 0;
    }
  }

  return -1;
}

static int append_chore_json(StringBuilder *sb, const void *item) {
  const Chore *chore = (const Chore *)item;
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
    "\"autoReassign\":%s,\"isDone\":%s,\"dueDateHistory\":%s,\"status\":\"%s\"}",
    id,
    title,
    assignee,
    assigned,
    due,
    chore->auto_reassign ? "true" : "false",
    chore->is_done ? "true" : "false",
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

  handlers_write_json_array(response, list.items, list.count, sizeof(Chore), append_chore_json, 1024, 200);
  db_free_chores(&list);
}

void handle_get_chore_by_id(const AppConfig *config, const char *id, HttpResponse *response) {
  ChoreList list;
  if (db_load_chores(config->chores_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read chores CSV");
    return;
  }

  size_t found_index = 0;
  if (find_chore_index(&list, id, &found_index) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 404, "Chore not found");
    return;
  }

  Chore *found = &list.items[found_index];

  handlers_write_json_item(response, found, append_chore_json, 512, 200);
  db_free_chores(&list);
}

void handle_create_chore(const AppConfig *config, const HttpRequest *request, HttpResponse *response) {
  char title[CHORE_TITLE_MAX];
  char assignee[CHORE_ASSIGNEE_MAX];
  char due_date[DATE_MAX];
  bool auto_reassign = false;
  bool random_assign = false;

  if (!json_get_string(request->body, "title", title, sizeof(title)) ||
      !json_get_string(request->body, "assignee", assignee, sizeof(assignee)) ||
      !json_get_string(request->body, "dueDate", due_date, sizeof(due_date))) {
    http_response_set_error(response, 400, "Missing required fields: title, assignee, dueDate");
    return;
  }

  trim_whitespace_in_place(title);
  trim_whitespace_in_place(assignee);
  trim_whitespace_in_place(due_date);
  json_get_bool(request->body, "autoReassign", &auto_reassign);
  json_get_bool(request->body, "randomAssign", &random_assign);

  if (title[0] == '\0' || assignee[0] == '\0') {
    http_response_set_error(response, 400, "Title and assignee must not be empty");
    return;
  }

  if (reassignment_validate_due_date(due_date) != 0) {
    http_response_set_error(response, 400, "Due date must be in YYYY-MM-DD format");
    return;
  }

  RoommateList roommates;
  if (db_load_roommates(config->roommates_csv_path, &roommates) != 0) {
    http_response_set_error(response, 500, "Could not read roommates CSV");
    return;
  }

  if (!random_assign && !db_roommate_exists(&roommates, assignee)) {
    db_free_roommates(&roommates);
    http_response_set_error(response, 400, "Assignee must be an existing roommate");
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

  char selected_assignee[CHORE_ASSIGNEE_MAX];
  char create_roll_message[MESSAGE_TEXT_MAX] = {0};
  int select_result = 0;
  if (random_assign) {
    select_result = reassignment_roll_create_assignment(
      &list,
      &roommates,
      due_date,
      selected_assignee,
      sizeof(selected_assignee),
      create_roll_message,
      sizeof(create_roll_message)
    );
  } else {
    snprintf(selected_assignee, sizeof(selected_assignee), "%s", assignee);
    select_result = 0;
  }

  if (select_result != 0) {
    db_free_roommates(&roommates);
    db_free_chores(&list);
    http_response_set_error(response, 500, "Could not balance assignee for week");
    return;
  }
  db_free_roommates(&roommates);

  db_next_chore_id(&list, chore->id, sizeof(chore->id));
  snprintf(chore->title, sizeof(chore->title), "%s", title);
  snprintf(chore->assignee, sizeof(chore->assignee), "%s", selected_assignee);

  // New chores are assigned for today by backend rule.
  date_today_plus_days_iso(0, chore->assigned_date, sizeof(chore->assigned_date));
  snprintf(chore->due_date, sizeof(chore->due_date), "%s", due_date);
  chore->auto_reassign = auto_reassign;

  chore->is_done = false;
  snprintf(chore->due_date_history, sizeof(chore->due_date_history), "%s", "[]");

  list.count += 1;

  if (db_save_chores(config->chores_csv_path, &list) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Could not write chores CSV");
    return;
  }

  if (random_assign && create_roll_message[0] != '\0') {
    if (handlers_append_message_record(config, create_roll_message, NULL) != 0) {
      fprintf(stderr, "Warning: could not write create roll message log\n");
    }
  }

  handlers_write_json_item(response, chore, append_chore_json, 512, 201);
  db_free_chores(&list);
}

void handle_patch_chore(const AppConfig *config, const char *id, const HttpRequest *request, HttpResponse *response) {
  ChoreList list;
  if (db_load_chores(config->chores_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read chores CSV");
    return;
  }

  size_t found_index = 0;
  if (find_chore_index(&list, id, &found_index) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 404, "Chore not found");
    return;
  }

  Chore *found = &list.items[found_index];

  int changed = 0;
  int is_done_patched = 0;
  bool bool_value = false;
  char due_date[DATE_MAX];
  char history[HISTORY_MAX];

  if (json_get_bool(request->body, "isDone", &bool_value)) {
    found->is_done = bool_value;
    is_done_patched = 1;
    changed = 1;
  }

  if (json_get_bool(request->body, "autoReassign", &bool_value)) {
    found->auto_reassign = bool_value;
    changed = 1;
  }

  if (json_get_string(request->body, "dueDate", due_date, sizeof(due_date))) {
    trim_whitespace_in_place(due_date);
    if (reassignment_validate_due_date(due_date) != 0) {
      db_free_chores(&list);
      http_response_set_error(response, 400, "Due date must be in YYYY-MM-DD format");
      return;
    }
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

  if (is_done_patched && found->is_done && found->auto_reassign) {
    RoommateList roommates;
    if (db_load_roommates(config->roommates_csv_path, &roommates) != 0) {
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not read roommates CSV");
      return;
    }

    if (roommates.count == 0) {
      db_free_roommates(&roommates);
      db_free_chores(&list);
      http_response_set_error(response, 400, "Auto reassign requires at least one roommate");
      return;
    }

    char next_due_date[DATE_MAX] = {0};
    date_today_plus_days_iso(7, next_due_date, sizeof(next_due_date));

    char winner_name[ROOMMATE_NAME_MAX] = {0};
    char reassignment_message[MESSAGE_TEXT_MAX] = {0};
    if (reassignment_roll_chore_assignment(
      found,
      &list,
      found_index,
      &roommates,
      next_due_date,
      winner_name,
      sizeof(winner_name),
      reassignment_message,
      sizeof(reassignment_message)
    ) != 0) {
      db_free_roommates(&roommates);
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not write reassignment log");
      return;
    }

    snprintf(found->assignee, sizeof(found->assignee), "%s", winner_name);
    date_today_plus_days_iso(0, found->assigned_date, sizeof(found->assigned_date));
    snprintf(found->due_date, sizeof(found->due_date), "%s", next_due_date);

    snprintf(found->due_date_history, sizeof(found->due_date_history), "%s", "[]");
    found->is_done = false;

    db_free_roommates(&roommates);

    if (db_save_chores(config->chores_csv_path, &list) != 0) {
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not write chores CSV");
      return;
    }

    if (handlers_append_message_record(config, reassignment_message, NULL) != 0) {
      fprintf(stderr, "Warning: could not write reassignment message log\n");
    }

    handlers_write_json_item(response, found, append_chore_json, 512, 200);
    db_free_chores(&list);
    return;
  }

  if (db_save_chores(config->chores_csv_path, &list) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Could not write chores CSV");
    return;
  }

  handlers_write_json_item(response, found, append_chore_json, 512, 200);
  db_free_chores(&list);
}

void handle_delete_chore(const AppConfig *config, const char *id, HttpResponse *response) {
  ChoreList list;
  if (db_load_chores(config->chores_csv_path, &list) != 0) {
    http_response_set_error(response, 500, "Could not read chores CSV");
    return;
  }

  size_t found_index = 0;
  if (find_chore_index(&list, id, &found_index) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 404, "Chore not found");
    return;
  }

  if (found_index + 1 < list.count) {
    memmove(&list.items[found_index], &list.items[found_index + 1], (list.count - found_index - 1) * sizeof(Chore));
  }
  list.count -= 1;

  if (db_save_chores(config->chores_csv_path, &list) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 500, "Could not write chores CSV");
    return;
  }

  db_free_chores(&list);
  http_response_set_json(response, 200, "{}");
}
