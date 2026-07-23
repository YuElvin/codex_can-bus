#include "signal_log_control.h"
#include "signal_log_time.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);             \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_fat_timestamp_range_leap_day_and_two_second_precision(void) {
  const uint64_t leap_day_ms = 1709251199000u; /* 2024-02-29T23:59:59Z */
  const uint32_t expected = ((uint32_t)(2024u - 1980u) << 25) | (2u << 21) | (29u << 16) |
                            (23u << 11) | (59u << 5) | 29u;

  ASSERT_TRUE(signal_log_time_fat_timestamp(0u, 0) == ((1u << 21) | (1u << 16)));
  ASSERT_TRUE(signal_log_time_fat_timestamp(leap_day_ms, 0) == expected);
  ASSERT_TRUE(signal_log_time_fat_timestamp(leap_day_ms - 3000u, 0) == expected - 1u);
  ASSERT_TRUE(signal_log_time_fat_timestamp(leap_day_ms, 840) ==
              (((uint32_t)(2024u - 1980u) << 25) | (3u << 21) | (1u << 16) | (13u << 11) |
               (59u << 5) | 29u));
  ASSERT_TRUE(signal_log_time_offset_is_valid(-720));
  ASSERT_TRUE(signal_log_time_offset_is_valid(840));
  ASSERT_TRUE(!signal_log_time_offset_is_valid(-721));
  ASSERT_TRUE(!signal_log_time_offset_is_valid(841));
  return 0;
}

static int test_local_filename_and_session_path_are_stable(void) {
  SignalLogControl control;
  char first_path[SIGNAL_LOG_PATH_MAX];

  signal_log_control_init(&control);
  ASSERT_TRUE(signal_log_control_set(&control, true, 250u, true, 1709251199123u, 1000u, 480));
  ASSERT_TRUE(strcmp(signal_log_control_session_path(&control),
                     "/log/20240301_075959123_signal-v2.csv") == 0);
  strcpy(first_path, signal_log_control_session_path(&control));
  ASSERT_TRUE(signal_log_control_sync_time(&control, 1709252200000u, 1200u, -480));
  ASSERT_TRUE(signal_log_control_set(&control, true, 1000u, false, 0u, 1300u, -480));
  ASSERT_TRUE(strcmp(signal_log_control_session_path(&control), first_path) == 0);
  ASSERT_TRUE(signal_log_control_set(&control, false, 1000u, false, 0u, 1400u, -480));
  ASSERT_TRUE(strcmp(signal_log_control_session_path(&control), first_path) == 0);
  ASSERT_TRUE(signal_log_control_set(&control, true, 1000u, false, 0u, 1401u, -480));
  ASSERT_TRUE(strcmp(signal_log_control_session_path(&control), first_path) != 0);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_fat_timestamp_range_leap_day_and_two_second_precision() == 0);
  ASSERT_TRUE(test_local_filename_and_session_path_are_stable() == 0);
  return 0;
}
