#ifndef DB_H
#define DB_H

#include "models.h"

int db_ensure_storage(const AppConfig *config);

int db_load_chores(const char *path, ChoreList *list);
int db_save_chores(const char *path, const ChoreList *list);
void db_free_chores(ChoreList *list);

int db_load_messages(const char *path, MessageList *list);
int db_save_messages(const char *path, const MessageList *list);
void db_free_messages(MessageList *list);

int db_next_chore_id(const ChoreList *list, char *out, size_t out_size);
int db_next_message_id(const MessageList *list, char *out, size_t out_size);

#endif
