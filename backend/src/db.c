#include "db.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <string.h>
#define strcasecmp _stricmp
#endif

#define CSV_FIELD_MAX 4096

static int file_exists(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  fclose(f);
  return 1;
}

static void copy_truncated(char *dst, size_t dst_size, const char *src) {
  if (!dst || dst_size == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }

  size_t n = strnlen(src, dst_size - 1);
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static void trim_eol(char *text) {
  text[strcspn(text, "\r\n")] = '\0';
}

static void csv_write_cell(FILE *f, const char *value) {
  int needs_quotes = 0;
  for (const char *p = value; *p; ++p) {
    if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
      needs_quotes = 1;
      break;
    }
  }

  if (!needs_quotes) {
    fputs(value, f);
    return;
  }

  fputc('"', f);
  for (const char *p = value; *p; ++p) {
    if (*p == '"') {
      fputc('"', f);
    }
    fputc(*p, f);
  }
  fputc('"', f);
}

static int csv_has_quoted_or_complex_cells(const char *line) {
  if (!line || line[0] == '\0') return 1;
  if (strchr(line, '"') != NULL) return 1;
  if (line[0] == ',') return 1;
  if (strstr(line, ",,") != NULL) return 1;

  size_t logical_len = strcspn(line, "\r\n");
  if (logical_len > 0 && line[logical_len - 1] == ',') return 1;
  return 0;
}

static char *csv_next_token(char *str, char **ctx) {
#ifdef _WIN32
  return strtok_s(str, ",", ctx);
#else
  return strtok_r(str, ",", ctx);
#endif
}

static int csv_split_line_simple(const char *line, char fields[][CSV_FIELD_MAX], int max_fields) {
  char buffer[16384];
  snprintf(buffer, sizeof(buffer), "%s", line);
  trim_eol(buffer);

  int count = 0;
  char *ctx = NULL;
  char *token = csv_next_token(buffer, &ctx);

  while (token && count < max_fields) {
    snprintf(fields[count], CSV_FIELD_MAX, "%s", token);
    ++count;
    token = csv_next_token(NULL, &ctx);
  }

  return count;
}

static int csv_split_line_quoted(const char *line, char fields[][CSV_FIELD_MAX], int max_fields) {
  memset(fields, 0, (size_t)max_fields * CSV_FIELD_MAX);

  int field = 0;
  size_t pos = 0;
  int in_quotes = 0;

  for (size_t i = 0; line[i] != '\0' && line[i] != '\n' && line[i] != '\r'; ++i) {
    char c = line[i];

    if (in_quotes) {
      if (c == '"') {
        if (line[i + 1] == '"') {
          if (pos + 1 < CSV_FIELD_MAX) fields[field][pos++] = '"';
          ++i;
        } else {
          in_quotes = 0;
        }
      } else {
        if (pos + 1 < CSV_FIELD_MAX) fields[field][pos++] = c;
      }
      continue;
    }

    if (c == '"') {
      in_quotes = 1;
    } else if (c == ',') {
      fields[field][pos] = '\0';
      ++field;
      if (field >= max_fields) return max_fields;
      pos = 0;
    } else {
      if (pos + 1 < CSV_FIELD_MAX) fields[field][pos++] = c;
    }
  }

  fields[field][pos] = '\0';
  return field + 1;
}

static int csv_split_line(const char *line, char fields[][CSV_FIELD_MAX], int max_fields) {
  memset(fields, 0, (size_t)max_fields * CSV_FIELD_MAX);

  if (!csv_has_quoted_or_complex_cells(line)) {
    return csv_split_line_simple(line, fields, max_fields);
  }

  return csv_split_line_quoted(line, fields, max_fields);
}

int db_ensure_storage(const AppConfig *config) {
  if (!file_exists(config->chores_csv_path)) {
    FILE *f = fopen(config->chores_csv_path, "w");
    if (!f) return -1;
    fputs("id,title,assignee,assignedDate,dueDate,isDone,isDeleted,dueDateHistory\n", f);
    fclose(f);
  }

  if (!file_exists(config->messages_csv_path)) {
    FILE *f = fopen(config->messages_csv_path, "w");
    if (!f) return -1;
    fputs("id,text,sentAt\n", f);
    fclose(f);
  }

  return 0;
}

int db_load_chores(const char *path, ChoreList *list) {
  list->items = NULL;
  list->count = 0;

  FILE *f = fopen(path, "r");
  if (!f) return -1;

  char line[16384];
  int first = 1;
  while (fgets(line, sizeof(line), f)) {
    if (first) {
      first = 0;
      continue;
    }

    char fields[8][CSV_FIELD_MAX];
    int count = csv_split_line(line, fields, 8);
    if (count < 8) continue;

    Chore *bigger = (Chore *)realloc(list->items, (list->count + 1) * sizeof(Chore));
    if (!bigger) {
      fclose(f);
      return -1;
    }
    list->items = bigger;

    Chore *chore = &list->items[list->count];
    memset(chore, 0, sizeof(*chore));

    copy_truncated(chore->id, sizeof(chore->id), fields[0]);
    copy_truncated(chore->title, sizeof(chore->title), fields[1]);
    copy_truncated(chore->assignee, sizeof(chore->assignee), fields[2]);
    copy_truncated(chore->assigned_date, sizeof(chore->assigned_date), fields[3]);
    copy_truncated(chore->due_date, sizeof(chore->due_date), fields[4]);
    chore->is_done = (strcmp(fields[5], "1") == 0 || strcmp(fields[5], "true") == 0);
    chore->is_deleted = (strcmp(fields[6], "1") == 0 || strcmp(fields[6], "true") == 0);
    copy_truncated(chore->due_date_history, sizeof(chore->due_date_history), fields[7]);

    ++list->count;
  }

  fclose(f);
  return 0;
}

int db_save_chores(const char *path, const ChoreList *list) {
  FILE *f = fopen(path, "w");
  if (!f) return -1;

  fputs("id,title,assignee,assignedDate,dueDate,isDone,isDeleted,dueDateHistory\n", f);

  for (size_t i = 0; i < list->count; ++i) {
    const Chore *c = &list->items[i];

    csv_write_cell(f, c->id); fputc(',', f);
    csv_write_cell(f, c->title); fputc(',', f);
    csv_write_cell(f, c->assignee); fputc(',', f);
    csv_write_cell(f, c->assigned_date); fputc(',', f);
    csv_write_cell(f, c->due_date); fputc(',', f);
    csv_write_cell(f, c->is_done ? "true" : "false"); fputc(',', f);
    csv_write_cell(f, c->is_deleted ? "true" : "false"); fputc(',', f);
    csv_write_cell(f, c->due_date_history[0] ? c->due_date_history : "[]");
    fputc('\n', f);
  }

  fclose(f);
  return 0;
}

void db_free_chores(ChoreList *list) {
  if (list->items) free(list->items);
  list->items = NULL;
  list->count = 0;
}

int db_load_messages(const char *path, MessageList *list) {
  list->items = NULL;
  list->count = 0;

  FILE *f = fopen(path, "r");
  if (!f) return -1;

  char line[8192];
  int first = 1;
  while (fgets(line, sizeof(line), f)) {
    if (first) {
      first = 0;
      continue;
    }

    char fields[3][CSV_FIELD_MAX];
    int count = csv_split_line(line, fields, 3);
    if (count < 3) continue;

    Message *bigger = (Message *)realloc(list->items, (list->count + 1) * sizeof(Message));
    if (!bigger) {
      fclose(f);
      return -1;
    }
    list->items = bigger;

    Message *msg = &list->items[list->count];
    memset(msg, 0, sizeof(*msg));
    copy_truncated(msg->id, sizeof(msg->id), fields[0]);
    copy_truncated(msg->text, sizeof(msg->text), fields[1]);
    copy_truncated(msg->sent_at, sizeof(msg->sent_at), fields[2]);
    ++list->count;
  }

  fclose(f);
  return 0;
}

int db_save_messages(const char *path, const MessageList *list) {
  FILE *f = fopen(path, "w");
  if (!f) return -1;

  fputs("id,text,sentAt\n", f);

  for (size_t i = 0; i < list->count; ++i) {
    const Message *m = &list->items[i];
    csv_write_cell(f, m->id); fputc(',', f);
    csv_write_cell(f, m->text); fputc(',', f);
    csv_write_cell(f, m->sent_at);
    fputc('\n', f);
  }

  fclose(f);
  return 0;
}

void db_free_messages(MessageList *list) {
  if (list->items) free(list->items);
  list->items = NULL;
  list->count = 0;
}

static int parse_id_number(const char *id, char prefix) {
  if (!id || id[0] != prefix || id[1] == '\0') return 0;

  char *end = NULL;
  long parsed = strtol(id + 1, &end, 10);
  if (end == id + 1 || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
    return 0;
  }

  return (int)parsed;
}

int db_next_chore_id(const ChoreList *list, char *out, size_t out_size) {
  int max_id = 0;
  for (size_t i = 0; i < list->count; ++i) {
    int value = parse_id_number(list->items[i].id, 'c');
    if (value > max_id) max_id = value;
  }
  return snprintf(out, out_size, "c%06d", max_id + 1) > 0 ? 0 : -1;
}

int db_next_message_id(const MessageList *list, char *out, size_t out_size) {
  int max_id = 0;
  for (size_t i = 0; i < list->count; ++i) {
    int value = parse_id_number(list->items[i].id, 'm');
    if (value > max_id) max_id = value;
  }
  return snprintf(out, out_size, "m%06d", max_id + 1) > 0 ? 0 : -1;
}
