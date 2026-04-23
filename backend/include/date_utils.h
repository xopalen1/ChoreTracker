#ifndef DATE_UTILS_H
#define DATE_UTILS_H

#include <stddef.h>

void date_today_plus_days_iso(int days, char *out, size_t out_size);
void date_now_iso_datetime(char *out, size_t out_size);
int date_validate_iso_date(const char *iso_date);
int date_week_bounds_from_iso_date(
	const char *iso_date,
	char *week_start,
	size_t week_start_size,
	char *week_end,
	size_t week_end_size
);

#endif
