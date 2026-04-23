#ifndef REASSIGNMENT_H
#define REASSIGNMENT_H

#include <stdbool.h>
#include <stddef.h>

#include "models.h"

int reassignment_validate_due_date(const char *due_date);

int reassignment_pick_balanced_assignee_for_due_date(
  const ChoreList *list,
  const RoommateList *roommates,
  const char *due_date,
  const char *preferred_assignee,
  size_t excluded_index,
  char *out_assignee,
  size_t out_assignee_size
);

int reassignment_roll_create_assignment(
  const ChoreList *list,
  const RoommateList *roommates,
  const char *due_date,
  char *out_assignee,
  size_t out_assignee_size,
  char *out_message,
  size_t out_message_size
);

int reassignment_roll_chore_assignment(
  Chore *chore,
  const ChoreList *list,
  size_t excluded_index,
  const RoommateList *roommates,
  const char *target_due_date,
  char *out_assignee,
  size_t out_assignee_size,
  char *out_message,
  size_t out_message_size
);

#endif
