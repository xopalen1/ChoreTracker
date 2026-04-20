#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stddef.h>

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} StringBuilder;

int sb_init(StringBuilder *sb, size_t initial_cap);
int sb_append(StringBuilder *sb, const char *text);
int sb_appendf(StringBuilder *sb, const char *fmt, ...);
void sb_free(StringBuilder *sb);

#endif
