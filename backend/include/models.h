#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>
#include <stddef.h>

#define CHORE_ID_MAX 32
#define CHORE_TITLE_MAX 128
#define CHORE_ASSIGNEE_MAX 64
#define DATE_MAX 16
#define DATETIME_MAX 32
#define HISTORY_MAX 4096

#define MESSAGE_ID_MAX 32
#define MESSAGE_TEXT_MAX 512

typedef struct {
  char id[CHORE_ID_MAX];
  char title[CHORE_TITLE_MAX];
  char assignee[CHORE_ASSIGNEE_MAX];
  char assigned_date[DATE_MAX];
  char due_date[DATE_MAX];
  bool is_done;
  bool is_deleted;
  char due_date_history[HISTORY_MAX];
} Chore;

typedef struct {
  Chore *items;
  size_t count;
} ChoreList;

typedef struct {
  char id[MESSAGE_ID_MAX];
  char text[MESSAGE_TEXT_MAX];
  char sent_at[DATETIME_MAX];
} Message;

typedef struct {
  Message *items;
  size_t count;
} MessageList;

typedef struct {
  int port;
  const char *chores_csv_path;
  const char *messages_csv_path;
} AppConfig;

#endif
