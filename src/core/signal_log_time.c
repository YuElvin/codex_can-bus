#include "signal_log_time.h"

#include <limits.h>
#include <stdio.h>

enum {
  MS_PER_SECOND = 1000u,
  MS_PER_MINUTE = 60000u,
  MS_PER_DAY = 86400000u,
};

static uint32_t fat_min_timestamp(void) {
  return (1u << 21) | (1u << 16);
}

bool signal_log_time_offset_is_valid(int32_t utc_offset_min) {
  return utc_offset_min >= SIGNAL_LOG_UTC_OFFSET_MIN_MIN &&
         utc_offset_min <= SIGNAL_LOG_UTC_OFFSET_MIN_MAX;
}

bool signal_log_time_local_calendar(uint64_t unix_ms,
                                    int32_t utc_offset_min,
                                    SignalLogCalendar *calendar) {
  uint64_t local_ms;
  uint64_t days;
  uint32_t day_ms;
  int64_t z;
  int64_t era;
  uint32_t doe;
  uint32_t yoe;
  int64_t year;
  uint32_t doy;
  uint32_t month_index;

  if (calendar == NULL || !signal_log_time_offset_is_valid(utc_offset_min)) {
    return false;
  }
  if (utc_offset_min < 0) {
    const uint64_t offset_ms = (uint64_t)(-utc_offset_min) * MS_PER_MINUTE;
    if (unix_ms < offset_ms) return false;
    local_ms = unix_ms - offset_ms;
  } else {
    const uint64_t offset_ms = (uint64_t)utc_offset_min * MS_PER_MINUTE;
    if (unix_ms > UINT64_MAX - offset_ms) return false;
    local_ms = unix_ms + offset_ms;
  }
  days = local_ms / MS_PER_DAY;
  day_ms = (uint32_t)(local_ms % MS_PER_DAY);
  if (days > (uint64_t)(INT64_MAX - 719468)) return false;
  z = (int64_t)days + 719468;
  era = (z >= 0 ? z : z - 146096) / 146097;
  doe = (uint32_t)(z - era * 146097);
  yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  year = (int64_t)yoe + era * 400;
  doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  month_index = (5 * doy + 2) / 153;
  year += month_index >= 10 ? 1 : 0;
  if (year < 0 || year > UINT16_MAX) return false;

  calendar->year = (uint16_t)year;
  calendar->month = (uint8_t)(month_index + (month_index < 10 ? 3 : -9));
  calendar->day = (uint8_t)(doy - (153 * month_index + 2) / 5 + 1);
  calendar->hour = (uint8_t)(day_ms / (60u * 60u * MS_PER_SECOND));
  day_ms %= 60u * 60u * MS_PER_SECOND;
  calendar->minute = (uint8_t)(day_ms / MS_PER_MINUTE);
  day_ms %= MS_PER_MINUTE;
  calendar->second = (uint8_t)(day_ms / MS_PER_SECOND);
  calendar->millisecond = (uint16_t)(day_ms % MS_PER_SECOND);
  return true;
}

uint32_t signal_log_time_fat_timestamp(uint64_t unix_ms, int32_t utc_offset_min) {
  SignalLogCalendar calendar;

  if (!signal_log_time_local_calendar(unix_ms, utc_offset_min, &calendar) || calendar.year < 1980u) {
    return fat_min_timestamp();
  }
  if (calendar.year > 2107u) {
    return (127u << 25) | (12u << 21) | (31u << 16) | (23u << 11) | (59u << 5) | 29u;
  }
  return ((uint32_t)(calendar.year - 1980u) << 25) |
         ((uint32_t)calendar.month << 21) |
         ((uint32_t)calendar.day << 16) |
         ((uint32_t)calendar.hour << 11) |
         ((uint32_t)calendar.minute << 5) |
         ((uint32_t)calendar.second / 2u);
}

bool signal_log_time_format_path(uint64_t unix_ms,
                                 int32_t utc_offset_min,
                                 char *path,
                                 size_t path_size) {
  SignalLogCalendar calendar;
  int written;

  if (path == NULL || path_size == 0u ||
      !signal_log_time_local_calendar(unix_ms, utc_offset_min, &calendar)) {
    return false;
  }
  written = snprintf(path, path_size, "/log/%04u%02u%02u_%02u%02u%02u%03u_signal-v2.csv",
                     (unsigned int)calendar.year, (unsigned int)calendar.month,
                     (unsigned int)calendar.day, (unsigned int)calendar.hour,
                     (unsigned int)calendar.minute, (unsigned int)calendar.second,
                     (unsigned int)calendar.millisecond);
  return written >= 0 && (size_t)written < path_size;
}
