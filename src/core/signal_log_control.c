#include "signal_log_control.h"

#include <stddef.h>
#include <string.h>

void signal_log_control_init(SignalLogControl *control) {
  if (control == NULL) {
    return;
  }
  control->enabled = false;
  control->time_synced = false;
  control->sample_period_ms = SIGNAL_LOG_SAMPLE_PERIOD_DEFAULT_MS;
  control->unix_base_ms = 0u;
  control->tick_base_ms = 0u;
  control->utc_offset_min = 0;
  control->session_start_unix_ms = 0u;
  control->session_utc_offset_min = 0;
  control->session_path[0] = '\0';
}

bool signal_log_control_sync_time(SignalLogControl *control,
                                  uint64_t unix_ms,
                                  uint32_t tick_ms,
                                  int32_t utc_offset_min) {
  if (control == NULL || !signal_log_time_offset_is_valid(utc_offset_min)) {
    return false;
  }
  control->unix_base_ms = unix_ms;
  control->tick_base_ms = tick_ms;
  control->utc_offset_min = (int16_t)utc_offset_min;
  control->time_synced = true;
  return true;
}

bool signal_log_control_set(SignalLogControl *control,
                            bool enabled,
                            uint32_t sample_period_ms,
                            bool has_unix_ms,
                            uint64_t unix_ms,
                            uint32_t tick_ms,
                            int32_t utc_offset_min) {
  SignalLogControl next;
  uint64_t session_start_unix_ms;

  if (control == NULL || sample_period_ms < SIGNAL_LOG_SAMPLE_PERIOD_MIN_MS ||
      sample_period_ms > SIGNAL_LOG_SAMPLE_PERIOD_MAX_MS ||
      !signal_log_time_offset_is_valid(utc_offset_min)) {
    return false;
  }
  next = *control;
  if (has_unix_ms && !signal_log_control_sync_time(&next, unix_ms, tick_ms, utc_offset_min)) {
    return false;
  }
  if (!has_unix_ms) {
    next.utc_offset_min = (int16_t)utc_offset_min;
  }
  if (enabled && !next.time_synced) {
    return false;
  }
  if (enabled && !next.enabled) {
    if (!signal_log_control_unix_ms(&next, tick_ms, &session_start_unix_ms) ||
        !signal_log_time_format_path(session_start_unix_ms, next.utc_offset_min,
                                     next.session_path, sizeof(next.session_path))) {
      return false;
    }
    next.session_start_unix_ms = session_start_unix_ms;
    next.session_utc_offset_min = next.utc_offset_min;
  }
  next.enabled = enabled;
  next.sample_period_ms = sample_period_ms;
  *control = next;
  return true;
}

bool signal_log_control_unix_ms(const SignalLogControl *control,
                                uint32_t tick_ms,
                                uint64_t *unix_ms) {
  if (control == NULL || unix_ms == NULL || !control->time_synced) {
    return false;
  }
  *unix_ms = control->unix_base_ms + (uint32_t)(tick_ms - control->tick_base_ms);
  return true;
}

const char *signal_log_control_session_path(const SignalLogControl *control) {
  return control == NULL ? "" : control->session_path;
}
