#include "reassignment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "date_utils.h"
#include "string_builder.h"

typedef struct {
  size_t roommate_index;
  int base_roll;
  int bonus;
  int total_roll;
} ReassignmentRoll;

int reassignment_validate_due_date(const char *due_date) {
  return date_validate_iso_date(due_date);
}

static size_t count_roommate_chores_in_week(
  const ChoreList *list,
  const char *roommate_name,
  const char *week_start,
  const char *week_end,
  size_t excluded_index
) {
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

static int collect_roll_eligible_indexes(
  const ChoreList *list,
  const RoommateList *roommates,
  const char *due_date,
  size_t excluded_index,
  size_t *eligible_indexes,
  size_t eligible_capacity,
  size_t *out_eligible_count,
  char *out_week_start,
  size_t out_week_start_size,
  char *out_week_end,
  size_t out_week_end_size
) {
  if (!list || !roommates || !due_date || !eligible_indexes || !out_eligible_count) {
    return -1;
  }

  if (roommates->count == 0 || eligible_capacity < roommates->count) {
    return -1;
  }

  char week_start[DATE_MAX];
  char week_end[DATE_MAX];
  if (date_week_bounds_from_iso_date(due_date, week_start, sizeof(week_start), week_end, sizeof(week_end)) != 0) {
    return -1;
  }

  size_t *counts = (size_t *)calloc(roommates->count, sizeof(size_t));
  if (!counts) {
    return -1;
  }

  size_t min_count = (size_t)-1;
  for (size_t i = 0; i < roommates->count; ++i) {
    size_t count = count_roommate_chores_in_week(list, roommates->items[i].name, week_start, week_end, excluded_index);
    counts[i] = count;
    if (count < min_count) {
      min_count = count;
    }
  }

  // Fairness rule: only roommates with the currently lowest weekly count roll.
  size_t eligible_count = 0;
  for (size_t i = 0; i < roommates->count; ++i) {
    if (counts[i] == min_count) {
      eligible_indexes[eligible_count++] = i;
    }
  }

  free(counts);

  if (eligible_count == 0) {
    return -1;
  }

  if (out_week_start && out_week_start_size > 0) {
    snprintf(out_week_start, out_week_start_size, "%s", week_start);
  }
  if (out_week_end && out_week_end_size > 0) {
    snprintf(out_week_end, out_week_end_size, "%s", week_end);
  }

  *out_eligible_count = eligible_count;
  return 0;
}

int reassignment_pick_balanced_assignee_for_due_date(
  const ChoreList *list,
  const RoommateList *roommates,
  const char *due_date,
  const char *preferred_assignee,
  size_t excluded_index,
  char *out_assignee,
  size_t out_assignee_size
) {
  if (!out_assignee || out_assignee_size == 0) {
    return -1;
  }

  size_t *eligible_indexes = (size_t *)calloc(roommates ? roommates->count : 0, sizeof(size_t));
  if (!eligible_indexes) {
    return -1;
  }

  size_t eligible_count = 0;
  int result = collect_roll_eligible_indexes(
    list,
    roommates,
    due_date,
    excluded_index,
    eligible_indexes,
    roommates->count,
    &eligible_count,
    NULL,
    0,
    NULL,
    0
  );
  if (result != 0) {
    free(eligible_indexes);
    return -1;
  }

  if (preferred_assignee && preferred_assignee[0] != '\0') {
    for (size_t i = 0; i < eligible_count; ++i) {
      const char *name = roommates->items[eligible_indexes[i]].name;
      if (strcmp(name, preferred_assignee) == 0) {
        snprintf(out_assignee, out_assignee_size, "%s", preferred_assignee);
        free(eligible_indexes);
        return 0;
      }
    }
  }

  snprintf(out_assignee, out_assignee_size, "%s", roommates->items[eligible_indexes[0]].name);
  free(eligible_indexes);
  return 0;
}

static int random_winner_slot(const int *rolls, size_t roll_count) {
  if (!rolls || roll_count == 0) {
    return -1;
  }

  int best_roll = -1;
  size_t tie_count = 0;
  for (size_t i = 0; i < roll_count; ++i) {
    if (rolls[i] > best_roll) {
      best_roll = rolls[i];
      tie_count = 1;
    } else if (rolls[i] == best_roll) {
      tie_count += 1;
    }
  }

  size_t selected_tie = (size_t)(rand() % (int)tie_count);
  size_t seen = 0;
  for (size_t i = 0; i < roll_count; ++i) {
    if (rolls[i] != best_roll) {
      continue;
    }

    if (seen == selected_tie) {
      return (int)i;
    }
    seen += 1;
  }

  return -1;
}

int reassignment_roll_create_assignment(
  const ChoreList *list,
  const RoommateList *roommates,
  const char *due_date,
  char *out_assignee,
  size_t out_assignee_size,
  char *out_message,
  size_t out_message_size
) {
  if (!list || !roommates || !due_date || !out_assignee || out_assignee_size == 0) {
    return -1;
  }

  size_t *eligible_indexes = (size_t *)calloc(roommates->count, sizeof(size_t));
  int *rolls = (int *)calloc(roommates->count, sizeof(int));
  if (!eligible_indexes || !rolls) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  size_t eligible_count = 0;
  char week_start[DATE_MAX] = {0};
  char week_end[DATE_MAX] = {0};
  if (collect_roll_eligible_indexes(
      list,
      roommates,
      due_date,
      (size_t)-1,
      eligible_indexes,
      roommates->count,
      &eligible_count,
      week_start,
      sizeof(week_start),
      week_end,
      sizeof(week_end)) != 0) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  for (size_t i = 0; i < eligible_count; ++i) {
    rolls[i] = (rand() % 20) + 1;
  }

  int winner_slot = random_winner_slot(rolls, eligible_count);
  if (winner_slot < 0) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  size_t winner_index = eligible_indexes[(size_t)winner_slot];
  snprintf(out_assignee, out_assignee_size, "%s", roommates->items[winner_index].name);

  if (out_message && out_message_size > 0) {
    StringBuilder sb;
    if (sb_init(&sb, 256) == 0) {
      if (sb_appendf(&sb, "Rollovanie pri vytvoreni pre tyzden %s az %s: ", week_start, week_end) == 0) {
        for (size_t i = 0; i < eligible_count; ++i) {
          if (i > 0 && sb_append(&sb, ", ") != 0) {
            break;
          }
          if (sb_appendf(&sb, "%s: %d", roommates->items[eligible_indexes[i]].name, rolls[i]) != 0) {
            break;
          }
        }
        sb_appendf(&sb, ". Vyherca: %s.", roommates->items[winner_index].name);
      }
      snprintf(out_message, out_message_size, "%s", sb.data ? sb.data : "");
      sb_free(&sb);
    }
  }

  free(eligible_indexes);
  free(rolls);
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

static int chore_set_roll_loss_bonuses_for_participants(
  Chore *chore,
  const RoommateList *roommates,
  const size_t *participant_indexes,
  size_t participant_count,
  size_t winner_index
) {
  if (!chore || !roommates) return -1;

  bool *is_participant = (bool *)calloc(roommates->count, sizeof(bool));
  if (!is_participant) {
    return -1;
  }

  for (size_t i = 0; i < participant_count; ++i) {
    if (participant_indexes[i] < roommates->count) {
      is_participant[participant_indexes[i]] = true;
    }
  }

  StringBuilder sb;
  if (sb_init(&sb, 128) != 0) {
    free(is_participant);
    return -1;
  }

  for (size_t i = 0; i < roommates->count; ++i) {
    const char *name = roommates->items[i].name;
    int previous_bonus = chore_get_roll_loss_bonus(chore, name);
    int next_bonus = previous_bonus;

    if (is_participant[i]) {
      next_bonus = (i == winner_index) ? 0 : (previous_bonus + 1);
    }

    if (next_bonus <= 0) {
      continue;
    }

    if (sb.len > 0 && sb_append(&sb, "|") != 0) {
      sb_free(&sb);
      free(is_participant);
      return -1;
    }

    if (sb_appendf(&sb, "%s=%d", name, next_bonus) != 0) {
      sb_free(&sb);
      free(is_participant);
      return -1;
    }
  }

  snprintf(chore->roll_loss_bonuses, sizeof(chore->roll_loss_bonuses), "%s", sb.data ? sb.data : "");
  sb_free(&sb);
  free(is_participant);
  return 0;
}

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
) {
  if (!chore || !list || !roommates || !target_due_date || !out_assignee || out_assignee_size == 0) {
    return -1;
  }

  size_t *eligible_indexes = (size_t *)calloc(roommates->count, sizeof(size_t));
  ReassignmentRoll *rolls = (ReassignmentRoll *)calloc(roommates->count, sizeof(ReassignmentRoll));
  if (!eligible_indexes || !rolls) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  size_t eligible_count = 0;
  if (collect_roll_eligible_indexes(
      list,
      roommates,
      target_due_date,
      excluded_index,
      eligible_indexes,
      roommates->count,
      &eligible_count,
      NULL,
      0,
      NULL,
      0) != 0) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  int *totals = (int *)calloc(eligible_count, sizeof(int));
  if (!totals) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  for (size_t i = 0; i < eligible_count; ++i) {
    size_t roommate_index = eligible_indexes[i];
    int base_roll = (rand() % 20) + 1;
    int bonus = chore_get_roll_loss_bonus(chore, roommates->items[roommate_index].name);
    int total = base_roll + bonus;

    rolls[i].roommate_index = roommate_index;
    rolls[i].base_roll = base_roll;
    rolls[i].bonus = bonus;
    rolls[i].total_roll = total;
    totals[i] = total;
  }

  int winner_slot = random_winner_slot(totals, eligible_count);
  free(totals);
  if (winner_slot < 0) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  size_t winner_roommate_index = rolls[(size_t)winner_slot].roommate_index;
  snprintf(out_assignee, out_assignee_size, "%s", roommates->items[winner_roommate_index].name);

  if (chore_set_roll_loss_bonuses_for_participants(chore, roommates, eligible_indexes, eligible_count, winner_roommate_index) != 0) {
    free(eligible_indexes);
    free(rolls);
    return -1;
  }

  if (out_message && out_message_size > 0) {
    StringBuilder sb;
    if (sb_init(&sb, 256) == 0) {
      if (sb_appendf(&sb, "Rollovanie pre \"%s\": ", chore->title) == 0) {
        for (size_t i = 0; i < eligible_count; ++i) {
          if (i > 0 && sb_append(&sb, ", ") != 0) {
            break;
          }

          if (sb_appendf(
              &sb,
              "%s: %d + %d = %d",
              roommates->items[rolls[i].roommate_index].name,
              rolls[i].base_roll,
              rolls[i].bonus,
              rolls[i].total_roll) != 0) {
            break;
          }
        }

        sb_appendf(&sb, ". Vyherca: %s.", roommates->items[winner_roommate_index].name);
      }
      snprintf(out_message, out_message_size, "%s", sb.data ? sb.data : "");
      sb_free(&sb);
    }
  }

  free(eligible_indexes);
  free(rolls);
  return 0;
}
