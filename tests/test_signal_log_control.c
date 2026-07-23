#include "signal_log_control.h"

#include <stdio.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);             \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_default_is_disabled_and_unsynced(void) {
  SignalLogControl control;
  signal_log_control_init(&control);
  ASSERT_TRUE(!control.enabled && !control.time_synced);
  ASSERT_TRUE(control.sample_period_ms == SIGNAL_LOG_SAMPLE_PERIOD_DEFAULT_MS);
  return 0;
}

static int test_start_requires_time_and_applies_period(void) {
  SignalLogControl control;
  uint64_t unix_ms = 0u;
  signal_log_control_init(&control);
  ASSERT_TRUE(!signal_log_control_set(&control, true, 100u, false, 0u, 1000u));
  ASSERT_TRUE(signal_log_control_set(&control, true, 250u, true, 1720000000123u, 1000u));
  ASSERT_TRUE(control.enabled && control.time_synced && control.sample_period_ms == 250u);
  ASSERT_TRUE(signal_log_control_unix_ms(&control, 1250u, &unix_ms));
  ASSERT_TRUE(unix_ms == 1720000000373u);
  ASSERT_TRUE(!signal_log_control_set(&control, true, 99u, false, 0u, 1250u));
  ASSERT_TRUE(!signal_log_control_set(&control, true, 10001u, false, 0u, 1250u));
  return 0;
}

int main(void) {
  if (test_default_is_disabled_and_unsynced() != 0) return 1;
  if (test_start_requires_time_and_applies_period() != 0) return 1;
  return 0;
}
