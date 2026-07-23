#ifndef SIGNAL_LOG_TIME_H
#define SIGNAL_LOG_TIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SIGNAL_LOG_UTC_OFFSET_MIN_MIN (-720)
#define SIGNAL_LOG_UTC_OFFSET_MIN_MAX 840
#define SIGNAL_LOG_PATH_MAX 48u

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t millisecond;
} SignalLogCalendar;

bool signal_log_time_offset_is_valid(int32_t utc_offset_min);
bool signal_log_time_local_calendar(uint64_t unix_ms,
                                    int32_t utc_offset_min,
                                    SignalLogCalendar *calendar);
uint32_t signal_log_time_fat_timestamp(uint64_t unix_ms, int32_t utc_offset_min);
bool signal_log_time_format_path(uint64_t unix_ms,
                                 int32_t utc_offset_min,
                                 char *path,
                                 size_t path_size);

#endif
