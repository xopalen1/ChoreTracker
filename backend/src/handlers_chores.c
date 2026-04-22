#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "date_utils.h"
#include "db.h"
#include "json_utils.h"
#include "string_builder.h"
#include "text_utils.h"

static const char *chore_status(const Chore *chore) {
  if (chore->is_done) return "done";
  return "open";
}

typedef struct {
  size_t index;
  int base_roll;
  int bonus;
  int total_roll;
} RoommateRoll;

static int parse_iso_date_local(const char *iso_date, struct tm *out) {
  if (!iso_date || !out) return -1;

  int year = 0;
  int month = 0;
  int day = 0;
  char tail = '\0';
  if (sscanf(iso_date, "%d-%d-%d%c", &year, &month, &day, &tail) != 3) {
    return -1;
  }

  if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
    return -1;
  }

  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = year - 1900;
  tmv.tm_mon = month - 1;
  tmv.tm_mday = day;
  tmv.tm_hour = 12;

  time_t normalized = mktime(&tmv);
  if (normalized == (time_t)-1) {
    return -1;
  }

  if (tmv.tm_year != year - 1900 || tmv.tm_mon != month - 1 || tmv.tm_mday != day) {
    return -1;
  }

  *out = tmv;
  return 0;
}

static int week_bounds_from_due_date(const char *due_date, char *week_start, size_t week_start_size, char *week_end, size_t week_end_size) {
  struct tm tmv;
  if (parse_iso_date_local(due_date, &tmv) != 0) {
    return -1;
  }

  int monday_offset = (tmv.tm_wday + 6) % 7;
  tmv.tm_mday -= monday_offset;
  if (mktime(&tmv) == (time_t)-1) {
    return -1;
  }

  if (strftime(week_start, week_start_size, "%Y-%m-%d", &tmv) == 0) {
    return -1;
  }

  tmv.tm_mday += 6;
  if (mktime(&tmv) == (time_t)-1) {
    return -1;
  }

  if (strftime(week_end, week_end_size, "%Y-%m-%d", &tmv) == 0) {
    return -1;
  }

  return 0;
}

static size_t count_roommate_chores_in_week(const ChoreList *list, const char *roommate_name, const char *week_start, const char *week_end, size_t excluded_index) {
  if (!list || !roommate_name || !week_start || !week_end) {
    return 0;
  }

  size_t count = 0;
  for (size_t i = 0; i < list->count; ++i) {
    if (i == excluded_index) {
      continue;
    }

    const Chore *chore = &list->items[i];
    if (strcmp(chore->assignee, roommate_name) != 0) {
      continue;
    }

    if (strcmp(chore->due_date, week_start) < 0 || strcmp(chore->due_date, week_end) > 0) {
      continue;
    }

    ++count;
  }

  return count;
}

static int pick_balanced_assignee_for_due_date(const ChoreList *list, const RoommateList *roommates, const char *due_date, const char *preferred_assignee, size_t excluded_index, char *out_assignee, size_t out_assignee_size) {
  if (!list || !roommates || !due_date || !out_assignee || out_assignee_size == 0) {
    return -1;
  }

  if (roommates->count == 0) {
    return -1;
  }

  char week_start[DATE_MAX];
  char week_end[DATE_MAX];
  if (week_bounds_from_due_date(due_date, week_start, sizeof(week_start), week_end, sizeof(week_end)) != 0) {
    return -1;
  }

  size_t min_count = (size_t)-1;
  size_t preferred_count = (size_t)-1;
  bool preferred_exists = false;
  size_t chosen_index = 0;

  for (size_t i = 0; i < roommates->count; ++i) {
    const char *name = roommates->items[i].name;
    size_t count = count_roommate_chores_in_week(list, name, week_start, week_end, excluded_index);

    if (preferred_assignee && preferred_assignee[0] != '\0' && strcmp(name, preferred_assignee) == 0) {
      preferred_exists = true;
      preferred_count = count;
    }

    if (count < min_count) {
      min_count = count;
      chosen_index = i;
    }
  }

  if (preferred_exists && preferred_count == min_count) {
    snprintf(out_assignee, out_assignee_size, "%s", preferred_assignee);
    return 0;
  }

  snprintf(out_assignee, out_assignee_size, "%s", roommates->items[chosen_index].name);
  return 0;
}

static int parse_bonus_value(const char *text, size_t len) {
  if (!text || len == 0) return 0;

  int value = 0;
  for (size_t i = 0; i < len; ++i) {
    char ch = text[i];
    if (ch < '0' || ch > '9') {
      return 0;
    }
    value = (value * 10) + (ch - '0');
  }

  return value;
}

static int chore_get_roll_loss_bonus(const Chore *chore, const char *roommate_name) {
  if (!chore || !roommate_name || roommate_name[0] == '\0') return 0;

  const char *cursor = chore->roll_loss_bonuses;
  size_t roommate_len = strlen(roommate_name);
  while (cursor && *cursor) {
    const char *entry_end = strchr(cursor, '|');
    if (!entry_end) entry_end = cursor + strlen(cursor);

    const char *sep = memchr(cursor, '=', (size_t)(entry_end - cursor));
    if (sep) {
      size_t key_len = (size_t)(sep - cursor);
      if (key_len == roommate_len && strncmp(cursor, roommate_name, key_len) == 0) {
        return parse_bonus_value(sep + 1, (size_t)(entry_end - (sep + 1)));
      }
    }

    if (*entry_end == '\0') {
      break;
    }
    cursor = entry_end + 1;
  }

  return 0;
}

static int chore_set_roll_loss_bonuses(Chore *chore, const RoommateList *roommates, size_t winner_index) {
  if (!chore || !roommates) return -1;

  StringBuilder sb;
  if (sb_init(&sb, 128) != 0) {
    return -1;
  }

  for (size_t i = 0; i < roommates->count; ++i) {
    const char *name = roommates->items[i].name;
    int previous_bonus = chore_get_roll_loss_bonus(chore, name);
    int next_bonus = (i == winner_index) ? 0 : (previous_bonus + 1);
    if (next_bonus <= 0) {
      continue;
    }

    if (sb.len > 0 && sb_append(&sb, "|") != 0) {
      sb_free(&sb);
      return -1;
    }

    if (sb_appendf(&sb, "%s=%d", name, next_bonus) != 0) {
      sb_free(&sb);
      return -1;
    }
  }

  snprintf(chore->roll_loss_bonuses, sizeof(chore->roll_loss_bonuses), "%s", sb.data ? sb.data : "");
  sb_free(&sb);
  return 0;
}

static int append_message_record(const AppConfig *config, const char *text) {
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

  int save_result = db_save_messages(config->messages_csv_path, &messages);
  db_free_messages(&messages);
  return save_result;
}

static int build_reassignment_message(const Chore *chore, const RoommateList *roommates, const RoommateRoll *rolls, size_t roll_count, size_t winner_index, int winning_roll, char *out, size_t out_size) {
  StringBuilder sb;
  if (sb_init(&sb, 256) != 0) {
    return -1;
  }

  if (sb_appendf(&sb, "Rollovanie pre \"%s\": ", chore->title) != 0) {
    sb_free(&sb);
    return -1;
  }

  for (size_t i = 0; i < roll_count; ++i) {
    if (i > 0 && sb_append(&sb, ", ") != 0) {
      sb_free(&sb);
      return -1;
    }

    if (sb_appendf(
      &sb,
      "%s: %d + %d = %d",
      roommates->items[rolls[i].index].name,
      rolls[i].base_roll,
      rolls[i].bonus,
      rolls[i].total_roll
    ) != 0) {
      sb_free(&sb);
      return -1;
    }
  }

  if (roll_count == 1) {
    if (sb_appendf(&sb, ". Výherca: %s.", roommates->items[winner_index].name) != 0) {
      sb_free(&sb);
      return -1;
    }
  } else {
    size_t tied_count = 0;
    for (size_t i = 0; i < roll_count; ++i) {
      if (rolls[i].total_roll == winning_roll) {
        ++tied_count;
      }
    }

    if (tied_count > 1) {
      if (sb_appendf(&sb, ". Remíza na %d medzi ", winning_roll) != 0) {
        sb_free(&sb);
        return -1;
      }

      size_t seen = 0;
      for (size_t i = 0; i < roll_count; ++i) {
        if (rolls[i].total_roll != winning_roll) {
          continue;
        }

        if (seen > 0 && sb_append(&sb, ", ") != 0) {
          sb_free(&sb);
          return -1;
        }

        if (sb_append(&sb, roommates->items[rolls[i].index].name) != 0) {
          sb_free(&sb);
          return -1;
        }

        ++seen;
      }

      if (sb_appendf(&sb, ". Výherca po remíze: %s.", roommates->items[winner_index].name) != 0) {
        sb_free(&sb);
        return -1;
      }
    } else if (sb_appendf(&sb, ". Výherca: %s.", roommates->items[winner_index].name) != 0) {
      sb_free(&sb);
      return -1;
    }
  }

  snprintf(out, out_size, "%s", sb.data);
  sb_free(&sb);
  return 0;
}

static int roll_auto_reassignment(Chore *chore, const RoommateList *roommates, char *winner_name, size_t winner_name_size, char *message_text, size_t message_text_size) {
  if (!roommates || roommates->count == 0) {
    return -1;
  }

  RoommateRoll *rolls = (RoommateRoll *)calloc(roommates->count, sizeof(RoommateRoll));
  size_t *tied_indexes = (size_t *)calloc(roommates->count, sizeof(size_t));
  if (!rolls || !tied_indexes) {
    free(rolls);
    free(tied_indexes);
    return -1;
  }

  int best_roll = -1;
  size_t tied_count = 0;

  for (size_t i = 0; i < roommates->count; ++i) {
    int base_roll = (rand() % 20) + 1;
    int bonus = chore_get_roll_loss_bonus(chore, roommates->items[i].name);
    int roll = base_roll + bonus;
    rolls[i].index = i;
    rolls[i].base_roll = base_roll;
    rolls[i].bonus = bonus;
    rolls[i].total_roll = roll;

    if (roll > best_roll) {
      best_roll = roll;
      tied_count = 0;
      tied_indexes[tied_count++] = i;
    } else if (roll == best_roll) {
      tied_indexes[tied_count++] = i;
    }
  }

  size_t winner_slot = tied_indexes[(size_t)(rand() % (int)tied_count)];
  const Roommate *winner = &roommates->items[winner_slot];
  snprintf(winner_name, winner_name_size, "%s", winner->name);

  if (chore_set_roll_loss_bonuses(chore, roommates, winner_slot) != 0) {
    free(rolls);
    free(tied_indexes);
    return -1;
  }

  int message_result = build_reassignment_message(chore, roommates, rolls, roommates->count, winner_slot, best_roll, message_text, message_text_size);

  free(rolls);
  free(tied_indexes);
  return message_result;
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

  size_t found_index = 0;
  if (find_chore_index(&list, id, &found_index) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 404, "Chore not found");
    return;
  }

  Chore *found = &list.items[found_index];

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
  bool auto_reassign = false;

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

  if (title[0] == '\0' || assignee[0] == '\0') {
    http_response_set_error(response, 400, "Title and assignee must not be empty");
    return;
  }

  char week_start[DATE_MAX];
  char week_end[DATE_MAX];
  if (week_bounds_from_due_date(due_date, week_start, sizeof(week_start), week_end, sizeof(week_end)) != 0) {
    http_response_set_error(response, 400, "Due date must be in YYYY-MM-DD format");
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

  char balanced_assignee[CHORE_ASSIGNEE_MAX];
  if (pick_balanced_assignee_for_due_date(&list, &roommates, due_date, assignee, (size_t)-1, balanced_assignee, sizeof(balanced_assignee)) != 0) {
    db_free_roommates(&roommates);
    db_free_chores(&list);
    http_response_set_error(response, 500, "Could not balance assignee for week");
    return;
  }
  db_free_roommates(&roommates);

  db_next_chore_id(&list, chore->id, sizeof(chore->id));
  snprintf(chore->title, sizeof(chore->title), "%s", title);
  snprintf(chore->assignee, sizeof(chore->assignee), "%s", balanced_assignee);

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

  size_t found_index = 0;
  if (find_chore_index(&list, id, &found_index) != 0) {
    db_free_chores(&list);
    http_response_set_error(response, 404, "Chore not found");
    return;
  }

  Chore *found = &list.items[found_index];

  int changed = 0;
  int due_date_patched = 0;
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
    char week_start[DATE_MAX];
    char week_end[DATE_MAX];
    if (week_bounds_from_due_date(due_date, week_start, sizeof(week_start), week_end, sizeof(week_end)) != 0) {
      db_free_chores(&list);
      http_response_set_error(response, 400, "Due date must be in YYYY-MM-DD format");
      return;
    }
    snprintf(found->due_date, sizeof(found->due_date), "%s", due_date);
    due_date_patched = 1;
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

    char winner_name[ROOMMATE_NAME_MAX] = {0};
    char reassignment_message[MESSAGE_TEXT_MAX] = {0};
    if (roll_auto_reassignment(found, &roommates, winner_name, sizeof(winner_name), reassignment_message, sizeof(reassignment_message)) != 0) {
      db_free_roommates(&roommates);
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not write reassignment log");
      return;
    }

    snprintf(found->assignee, sizeof(found->assignee), "%s", winner_name);
    date_today_plus_days_iso(0, found->assigned_date, sizeof(found->assigned_date));
    date_today_plus_days_iso(7, found->due_date, sizeof(found->due_date));

    char balanced_assignee[CHORE_ASSIGNEE_MAX];
    if (pick_balanced_assignee_for_due_date(&list, &roommates, found->due_date, found->assignee, found_index, balanced_assignee, sizeof(balanced_assignee)) != 0) {
      db_free_roommates(&roommates);
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not balance assignee for week");
      return;
    }
    snprintf(found->assignee, sizeof(found->assignee), "%s", balanced_assignee);

    snprintf(found->due_date_history, sizeof(found->due_date_history), "%s", "[]");
    found->is_done = false;

    db_free_roommates(&roommates);

    if (db_save_chores(config->chores_csv_path, &list) != 0) {
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not write chores CSV");
      return;
    }

    if (append_message_record(config, reassignment_message) != 0) {
      fprintf(stderr, "Warning: could not write reassignment message log\n");
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
    return;
  }

  if (due_date_patched) {
    RoommateList roommates;
    if (db_load_roommates(config->roommates_csv_path, &roommates) != 0) {
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not read roommates CSV");
      return;
    }

    if (roommates.count == 0) {
      db_free_roommates(&roommates);
      db_free_chores(&list);
      http_response_set_error(response, 400, "At least one roommate is required");
      return;
    }

    char balanced_assignee[CHORE_ASSIGNEE_MAX];
    if (pick_balanced_assignee_for_due_date(&list, &roommates, found->due_date, found->assignee, found_index, balanced_assignee, sizeof(balanced_assignee)) != 0) {
      db_free_roommates(&roommates);
      db_free_chores(&list);
      http_response_set_error(response, 500, "Could not balance assignee for week");
      return;
    }

    snprintf(found->assignee, sizeof(found->assignee), "%s", balanced_assignee);
    db_free_roommates(&roommates);
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
