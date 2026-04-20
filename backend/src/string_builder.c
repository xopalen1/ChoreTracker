#include "string_builder.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int sb_init(StringBuilder *sb, size_t initial_cap) {
  sb->data = (char *)malloc(initial_cap);
  if (!sb->data) return -1;
  sb->data[0] = '\0';
  sb->len = 0;
  sb->cap = initial_cap;
  return 0;
}

int sb_append(StringBuilder *sb, const char *text) {
  size_t n = strlen(text);
  if (sb_reserve(sb, n) != 0) return -1;
  memcpy(sb->data + sb->len, text, n);
  sb->len += n;
  sb->data[sb->len] = '\0';
  return 0;
}

int sb_appendf(StringBuilder *sb, const char *fmt, ...) {
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

void sb_free(StringBuilder *sb) {
  free(sb->data);
  sb->data = NULL;
  sb->len = 0;
  sb->cap = 0;
}
