#include "json_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *find_key(const char *json, const char *key) {
  static char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  return strstr(json, pattern);
}

static const char *value_start(const char *json, const char *key) {
  const char *key_pos = find_key(json, key);
  if (!key_pos) return NULL;

  const char *colon = strchr(key_pos, ':');
  if (!colon) return NULL;
  colon++;

  while (*colon == ' ' || *colon == '\t' || *colon == '\n' || *colon == '\r') {
    ++colon;
  }
  return colon;
}

bool json_get_string(const char *json, const char *key, char *out, size_t out_size) {
  const char *start = value_start(json, key);
  if (!start || *start != '"') return false;
  ++start;

  size_t w = 0;
  for (const char *p = start; *p; ++p) {
    if (*p == '"') {
      out[w] = '\0';
      return true;
    }

    if (*p == '\\' && p[1]) {
      ++p;
      if (w + 1 < out_size) out[w++] = *p;
      continue;
    }

    if (w + 1 < out_size) out[w++] = *p;
  }

  return false;
}

bool json_get_bool(const char *json, const char *key, bool *out) {
  const char *start = value_start(json, key);
  if (!start) return false;

  if (strncmp(start, "true", 4) == 0) {
    *out = true;
    return true;
  }

  if (strncmp(start, "false", 5) == 0) {
    *out = false;
    return true;
  }

  return false;
}

bool json_get_array_raw(const char *json, const char *key, char *out, size_t out_size) {
  const char *start = value_start(json, key);
  if (!start || *start != '[') return false;

  int depth = 0;
  bool in_string = false;
  size_t w = 0;

  for (const char *p = start; *p; ++p) {
    char c = *p;

    if (c == '"' && (p == start || p[-1] != '\\')) {
      in_string = !in_string;
    }

    if (!in_string) {
      if (c == '[') ++depth;
      if (c == ']') --depth;
    }

    if (w + 1 < out_size) out[w++] = c;

    if (depth == 0) {
      out[w] = '\0';
      return true;
    }
  }

  return false;
}

void json_escape(const char *src, char *dst, size_t dst_size) {
  size_t w = 0;
  for (size_t i = 0; src[i] != '\0' && w + 2 < dst_size; ++i) {
    unsigned char c = (unsigned char)src[i];

    if (c == '"' || c == '\\') {
      dst[w++] = '\\';
      dst[w++] = (char)c;
      continue;
    }

    if (c == '\n') {
      dst[w++] = '\\';
      dst[w++] = 'n';
      continue;
    }

    if (c == '\r') {
      dst[w++] = '\\';
      dst[w++] = 'r';
      continue;
    }

    if (c < 32) {
      continue;
    }

    dst[w++] = (char)c;
  }

  dst[w] = '\0';
}
