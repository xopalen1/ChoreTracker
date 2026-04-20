#include "date_utils.h"

#include <stdio.h>
#include <time.h>

void date_today_plus_days_iso(int days, char *out, size_t out_size) {
  time_t now = time(NULL);
  struct tm tmv;
#ifdef _WIN32
  localtime_s(&tmv, &now);
#else
  localtime_r(&now, &tmv);
#endif

  tmv.tm_mday += days;
  mktime(&tmv);
  strftime(out, out_size, "%Y-%m-%d", &tmv);
}

void date_now_iso_datetime(char *out, size_t out_size) {
  time_t now = time(NULL);
  struct tm tmv;
#ifdef _WIN32
  gmtime_s(&tmv, &now);
#else
  gmtime_r(&now, &tmv);
#endif
  strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}
