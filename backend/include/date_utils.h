#ifndef DATE_UTILS_H
#define DATE_UTILS_H

#include <stddef.h>

void date_today_iso(char *out, size_t out_size);
void date_next_sunday_iso(char *out, size_t out_size);
void date_now_iso_datetime(char *out, size_t out_size);

#endif
