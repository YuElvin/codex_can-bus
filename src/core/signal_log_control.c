#include "signal_log_control.h"

#include <stddef.h>

void signal_log_control_init(SignalLogControl *control) {
  if (control == NULL) {
    return;
  }
  control->enabled = false;
  control->time_synced = false;
  control->sample_period_ms = SIGNAL_LOG_SAMPLE_PERIOD_DEFAULT_MS;
  control->unix_base_ms = 0u;
  control->tick_base_ms = 0u;
}

bool signal_log_control_sync_time(SignalLogControl *control, uint64_t unix_ms, uint32_t tick_ms) {
  if (control == NULL) {
    return false;
  }
  control->unix_base_ms = unix_ms;
  control->tick_base_ms = tick_ms;
  control->time_synced = true;
  return true;
}

bool signal_log_control_set(SignalLogControl *control,
                            bool enabled,
                            uint32_t sample_period_ms,
                            bool has_unix_ms,
                            uint64_t unix_ms,
                            uint32_t tick_ms) {
  if (control == NULL || sample_period_ms < SIGNAL_LOG_SAMPLE_PERIOD_MIN_MS ||
      sample_period_ms > SIGNAL_LOG_SAMPLE_PERIOD_MAX_MS) {
    return false;
  }
  if (has_unix_ms && !signal_log_control_sync_time(control, unix_ms, tick_ms)) {
    return false;
  }
  if (enabled && !control->time_synced) {
    return false;
  }
  control->enabled = enabled;
  control->sample_period_ms = sample_period_ms;
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
