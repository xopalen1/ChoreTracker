#include "text_utils.h"

#include <ctype.h>
#include <string.h>

void trim_whitespace_in_place(char *text) {
  if (!text || text[0] == '\0') return;

  size_t start = 0;
  while (text[start] != '\0' && isspace((unsigned char)text[start])) {
    ++start;
  }

  if (start > 0) {
    memmove(text, text + start, strlen(text + start) + 1);
  }

  size_t len = strlen(text);
  while (len > 0 && isspace((unsigned char)text[len - 1])) {
    text[len - 1] = '\0';
    --len;
  }
}
