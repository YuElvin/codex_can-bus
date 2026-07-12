#include "rule_file.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_valid_file(void) {
  static const char text[] =
    "version=1\r\n"
    "onThreshold=42434\n"
    "offThreshold=42432\n"
    "delayMs=1000\n"
    "timeoutMs=1500\n";
  RuleTaskConfig config = {0};

  ASSERT_TRUE(rule_file_parse_v1((const uint8_t *)text, strlen(text), &config));
  ASSERT_TRUE(config.on_threshold == 42434.0);
  ASSERT_TRUE(config.off_threshold == 42432.0);
  ASSERT_TRUE(config.delay_ms == 1000u);
  ASSERT_TRUE(config.timeout_ms == 1500u);
  return 0;
}

static int test_missing_required_field_does_not_change_candidate(void) {
  static const char text[] =
    "version=1\n"
    "onThreshold=42434\n"
    "offThreshold=42432\n"
    "delayMs=1000\n";
  const RuleTaskConfig before = {
    .on_threshold = 7.0,
    .off_threshold = 3.0,
    .delay_ms = 11u,
    .timeout_ms = 22u,
  };
  RuleTaskConfig candidate = before;

  ASSERT_TRUE(!rule_file_parse_v1((const uint8_t *)text, strlen(text), &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

static int test_invalid_threshold_does_not_change_candidate(void) {
  static const char text[] =
    "version=1\n"
    "onThreshold=10\n"
    "offThreshold=10\n"
    "delayMs=1000\n"
    "timeoutMs=1500\n";
  const RuleTaskConfig before = {
    .on_threshold = 7.0,
    .off_threshold = 3.0,
    .delay_ms = 11u,
    .timeout_ms = 22u,
  };
  RuleTaskConfig candidate = before;

  ASSERT_TRUE(!rule_file_parse_v1((const uint8_t *)text, strlen(text), &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

static int test_invalid_timing_does_not_change_candidate(void) {
  static const char text[] =
    "version=1\n"
    "onThreshold=10\n"
    "offThreshold=9\n"
    "delayMs=1501\n"
    "timeoutMs=1500\n";
  const RuleTaskConfig before = {
    .on_threshold = 7.0,
    .off_threshold = 3.0,
    .delay_ms = 11u,
    .timeout_ms = 22u,
  };
  RuleTaskConfig candidate = before;

  ASSERT_TRUE(!rule_file_parse_v1((const uint8_t *)text, strlen(text), &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_valid_file() == 0);
  ASSERT_TRUE(test_missing_required_field_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_invalid_threshold_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_invalid_timing_does_not_change_candidate() == 0);
  return 0;
}
