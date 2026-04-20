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
#define ROLL_BONUS_MAX 2048

#define MESSAGE_ID_MAX 32
#define MESSAGE_TEXT_MAX 512

#define ROOMMATE_ID_MAX 32
#define ROOMMATE_NAME_MAX 64

typedef struct {
  char id[CHORE_ID_MAX];
  char title[CHORE_TITLE_MAX];
  char assignee[CHORE_ASSIGNEE_MAX];
  char assigned_date[DATE_MAX];
  char due_date[DATE_MAX];
  bool auto_reassign;
  bool is_done;
  char due_date_history[HISTORY_MAX];
  char roll_loss_bonuses[ROLL_BONUS_MAX];
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
  char id[ROOMMATE_ID_MAX];
  char name[ROOMMATE_NAME_MAX];
} Roommate;

typedef struct {
  Roommate *items;
  size_t count;
} RoommateList;

typedef struct {
  int port;
  const char *chores_csv_path;
  const char *messages_csv_path;
  const char *roommates_csv_path;
} AppConfig;

#endif
