#include "signal_log_buffer.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static const SignalCacheEntry entry = {
  .key = "Can2Data.marker",
  .unit = "count",
  .physical_value = 42.0,
  .raw_value = 42,
  .updated_ms = 100u,
  .quality = SIGNAL_QUALITY_OK,
};

static int test_normal_threshold_and_timed_flush(void) {
  char storage[256];
  SignalLogBuffer buffer;
  signal_log_buffer_init(&buffer, storage, sizeof(storage));

  ASSERT_TRUE(signal_log_buffer_append_snapshot(&buffer, &entry, 1u, true) == SIGNAL_LOG_BUFFER_OK);
  ASSERT_TRUE(strstr(storage, "updated_ms,key,value,raw,unit,quality\n") == storage);
  ASSERT_TRUE(!signal_log_buffer_should_flush(&buffer, 200u, 100u, 200u, 5000u));
  ASSERT_TRUE(signal_log_buffer_should_flush(&buffer, 1u, 100u, 200u, 5000u));
  ASSERT_TRUE(signal_log_buffer_should_flush(&buffer, 200u, 100u, 5100u, 5000u));

  signal_log_buffer_clear(&buffer);
  ASSERT_TRUE(buffer.length == 0u);
  ASSERT_TRUE(storage[0] == '\0');
  ASSERT_TRUE(!signal_log_buffer_should_flush(&buffer, 1u, 0u, 6000u, 5000u));
  return 0;
}

static int test_capacity_rejects_without_corrupting_buffer(void) {
  char storage[80];
  char before[80];
  SignalLogBuffer buffer;
  signal_log_buffer_init(&buffer, storage, sizeof(storage));

  ASSERT_TRUE(signal_log_buffer_append_snapshot(&buffer, &entry, 1u, false) == SIGNAL_LOG_BUFFER_OK);
  memcpy(before, storage, sizeof(before));
  const size_t length = buffer.length;
  ASSERT_TRUE(signal_log_buffer_append_snapshot(&buffer, &entry, 1u, false) == SIGNAL_LOG_BUFFER_FULL);
  ASSERT_TRUE(buffer.length == length);
  ASSERT_TRUE(memcmp(storage, before, sizeof(storage)) == 0);
  return 0;
}

static int test_path_selection_uses_recovery_only_for_default_path_errors(void) {
  ASSERT_TRUE(signal_log_select_path(0) == SIGNAL_LOG_PATH_DEFAULT);
  ASSERT_TRUE(signal_log_select_path(SIGNAL_LOG_FILE_NOT_FOUND) == SIGNAL_LOG_PATH_DEFAULT);
  ASSERT_TRUE(signal_log_select_path(1) == SIGNAL_LOG_PATH_RECOVERY);
  ASSERT_TRUE(signal_log_select_path(9) == SIGNAL_LOG_PATH_RECOVERY);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_normal_threshold_and_timed_flush() == 0);
  ASSERT_TRUE(test_capacity_rejects_without_corrupting_buffer() == 0);
  ASSERT_TRUE(test_path_selection_uses_recovery_only_for_default_path_errors() == 0);
  return 0;
}
