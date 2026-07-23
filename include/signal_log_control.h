#ifndef SIGNAL_LOG_CONTROL_H
#define SIGNAL_LOG_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_log_time.h"

#define SIGNAL_LOG_SAMPLE_PERIOD_MIN_MS 100u
#define SIGNAL_LOG_SAMPLE_PERIOD_MAX_MS 10000u
#define SIGNAL_LOG_SAMPLE_PERIOD_DEFAULT_MS 1000u

typedef struct {
  bool enabled;
  bool time_synced;
  uint32_t sample_period_ms;
  uint64_t unix_base_ms;
  uint32_t tick_base_ms;
  int16_t utc_offset_min;
  uint64_t session_start_unix_ms;
  int16_t session_utc_offset_min;
  char session_path[SIGNAL_LOG_PATH_MAX];
} SignalLogControl;

void signal_log_control_init(SignalLogControl *control);
bool signal_log_control_sync_time(SignalLogControl *control,
                                  uint64_t unix_ms,
                                  uint32_t tick_ms,
                                  int32_t utc_offset_min);
bool signal_log_control_set(SignalLogControl *control,
                            bool enabled,
                            uint32_t sample_period_ms,
                            bool has_unix_ms,
                            uint64_t unix_ms,
                            uint32_t tick_ms,
                            int32_t utc_offset_min);
bool signal_log_control_unix_ms(const SignalLogControl *control,
                                uint32_t tick_ms,
                                uint64_t *unix_ms);
const char *signal_log_control_session_path(const SignalLogControl *control);

#endif
