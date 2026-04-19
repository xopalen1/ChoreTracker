#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdbool.h>
#include <stddef.h>

bool json_get_string(const char *json, const char *key, char *out, size_t out_size);
bool json_get_bool(const char *json, const char *key, bool *out);
bool json_get_array_raw(const char *json, const char *key, char *out, size_t out_size);
void json_escape(const char *src, char *dst, size_t dst_size);

#endif
