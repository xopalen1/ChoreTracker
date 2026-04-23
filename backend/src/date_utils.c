#include "date_utils.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int parse_iso_date_local(const char *iso_date, struct tm *out) {
  if (!iso_date || !out) return -1;

  int year = 0;
  int month = 0;
  int day = 0;
  char tail = '\0';
  if (sscanf(iso_date, "%d-%d-%d%c", &year, &month, &day, &tail) != 3) {
    return -1;
  }

  if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
    return -1;
  }

  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = year - 1900;
  tmv.tm_mon = month - 1;
  tmv.tm_mday = day;
  tmv.tm_hour = 12;

  time_t normalized = mktime(&tmv);
  if (normalized == (time_t)-1) {
    return -1;
  }

  if (tmv.tm_year != year - 1900 || tmv.tm_mon != month - 1 || tmv.tm_mday != day) {
    return -1;
  }

  *out = tmv;
  return 0;
}

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

int date_validate_iso_date(const char *iso_date) {
  struct tm tmv;
  return parse_iso_date_local(iso_date, &tmv);
}

int date_week_bounds_from_iso_date(
  const char *iso_date,
  char *week_start,
  size_t week_start_size,
  char *week_end,
  size_t week_end_size
) {
  struct tm tmv;
  if (parse_iso_date_local(iso_date, &tmv) != 0) {
    return -1;
  }

  int monday_offset = (tmv.tm_wday + 6) % 7;
  tmv.tm_mday -= monday_offset;
  if (mktime(&tmv) == (time_t)-1) {
    return -1;
  }

  if (strftime(week_start, week_start_size, "%Y-%m-%d", &tmv) == 0) {
    return -1;
  }

  tmv.tm_mday += 6;
  if (mktime(&tmv) == (time_t)-1) {
    return -1;
  }

  if (strftime(week_end, week_end_size, "%Y-%m-%d", &tmv) == 0) {
    return -1;
  }

  return 0;
}
