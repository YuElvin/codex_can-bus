#include "rule_file.h"

#include <math.h>
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

static const char default_v2_text[] =
  "version=2\r\n"
  "ruleCount=2\n"
  "\n"
  "rule0.relay=0\n"
  "rule0.threshold=42434\n"
  "rule0.action=on\n"
  "rule0.delayMs=1000\n"
  "rule0.timeoutMs=1500\n"
  "rule0.safeState=off\n"
  "rule0.priority=10\n"
  "rule1.relay=0\n"
  "rule1.threshold=42435\n"
  "rule1.action=off\n"
  "rule1.delayMs=0\n"
  "rule1.timeoutMs=1500\n"
  "rule1.safeState=off\n"
  "rule1.priority=20\n";

static int test_valid_v2_default(void) {
  RuleEngine engine;

  ASSERT_TRUE(rule_file_parse_v2((const uint8_t *)default_v2_text,
                                 strlen(default_v2_text),
                                 &engine));
  ASSERT_TRUE(engine.rule_count == 2u);
  ASSERT_TRUE(strcmp(engine.rules[0].signal_key, "Can2Data.marker") == 0);
  ASSERT_TRUE(engine.rules[0].op == RULE_OP_GE);
  ASSERT_TRUE(engine.rules[0].threshold == 42434.0);
  ASSERT_TRUE(engine.rules[0].action_state == RELAY_STATE_ON);
  ASSERT_TRUE(engine.rules[0].delay_ms == 1000u);
  ASSERT_TRUE(engine.rules[0].timeout_ms == 1500u);
  ASSERT_TRUE(engine.rules[0].safe_state == RELAY_STATE_OFF);
  ASSERT_TRUE(engine.rules[0].priority == 10u);
  ASSERT_TRUE(engine.rules[1].threshold == 42435.0);
  ASSERT_TRUE(engine.rules[1].action_state == RELAY_STATE_OFF);
  ASSERT_TRUE(engine.rules[1].delay_ms == 0u);
  ASSERT_TRUE(engine.rules[1].priority == 20u);
  return 0;
}

static int assert_invalid_v2_does_not_change_candidate(const char *text) {
  RuleEngine before;
  RuleEngine candidate;

  rule_engine_init(&before);
  before.relay_defaults[0] = RELAY_STATE_ON;
  candidate = before;
  ASSERT_TRUE(!rule_file_parse_v2((const uint8_t *)text, strlen(text), &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

static int test_invalid_v2_classes_do_not_change_candidate(void) {
  static const char unknown[] = "version=2\nruleCount=2\nunknown=1\n";
  static const char duplicate[] =
    "version=2\nruleCount=2\nrule0.relay=0\nrule0.relay=0\n";
  static const char missing[] = "version=2\nruleCount=2\n";
  static const char non_decimal[] =
    "version=2\nruleCount=2\nrule0.relay=0\nrule0.threshold=x\n";
  static const char overflow[] =
    "version=2\nruleCount=2\nrule0.relay=0\nrule0.threshold=4294967296\n";
  static const char invalid_relay[] =
    "version=2\nruleCount=2\nrule0.relay=2\n";
  static const char invalid_state[] =
    "version=2\nruleCount=2\nrule0.action=enable\n";
  static const char same_priority[] =
    "version=2\nruleCount=2\n"
    "rule0.relay=0\nrule0.threshold=1\nrule0.action=on\nrule0.delayMs=0\n"
    "rule0.timeoutMs=1\nrule0.safeState=off\nrule0.priority=10\n"
    "rule1.relay=1\nrule1.threshold=2\nrule1.action=off\nrule1.delayMs=0\n"
    "rule1.timeoutMs=1\nrule1.safeState=off\nrule1.priority=10\n";
  static const char invalid_timing[] =
    "version=2\nruleCount=2\n"
    "rule0.relay=0\nrule0.threshold=1\nrule0.action=on\nrule0.delayMs=2\n"
    "rule0.timeoutMs=1\nrule0.safeState=off\nrule0.priority=10\n"
    "rule1.relay=1\nrule1.threshold=2\nrule1.action=off\nrule1.delayMs=0\n"
    "rule1.timeoutMs=1\nrule1.safeState=off\nrule1.priority=20\n";

  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(unknown) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(duplicate) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(missing) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(non_decimal) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(overflow) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(invalid_relay) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(invalid_state) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(same_priority) == 0);
  ASSERT_TRUE(assert_invalid_v2_does_not_change_candidate(invalid_timing) == 0);
  return 0;
}

static int test_oversized_v2_does_not_change_candidate(void) {
  uint8_t text[513];
  RuleEngine before;
  RuleEngine candidate;

  memset(text, (int)'\n', sizeof(text));
  rule_engine_init(&before);
  before.relay_defaults[0] = RELAY_STATE_ON;
  candidate = before;
  ASSERT_TRUE(!rule_file_parse_v2(text, sizeof(text), &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

static const char default_v3_text[] =
  "version=3\nruleCount=2\n"
  "rule0.enabled=1\nrule0.relay=0\nrule0.threshold=42434\nrule0.action=on\n"
  "rule0.delayMs=1000\nrule0.timeoutMs=1500\nrule0.safeState=off\nrule0.priority=10\n"
  "rule1.enabled=0\nrule1.relay=0\nrule1.threshold=42435\nrule1.action=off\n"
  "rule1.delayMs=0\nrule1.timeoutMs=1500\nrule1.safeState=off\nrule1.priority=20\n";

static int test_valid_v3_and_enabled_engine(void) {
  RuleFileV3 rules;
  RuleEngine engine;
  char text[RULE_FILE_V3_MAX_BYTES + 1u];
  RuleFileV3 round_trip;

  ASSERT_TRUE(rule_file_parse_v3((const uint8_t *)default_v3_text, strlen(default_v3_text), &rules));
  ASSERT_TRUE(rules.slots[0].enabled && !rules.slots[1].enabled);
  const size_t len = rule_file_format_v3(&rules, text, sizeof(text));
  ASSERT_TRUE(len > 0u && len <= RULE_FILE_V3_MAX_BYTES);
  ASSERT_TRUE(rule_file_parse_v3((const uint8_t *)text, len, &round_trip));
  ASSERT_TRUE(memcmp(&round_trip, &rules, sizeof(rules)) == 0);
  ASSERT_TRUE(rule_file_v3_build_engine(&rules, &engine));
  ASSERT_TRUE(engine.rule_count == 1u);
  ASSERT_TRUE(engine.rules[0].threshold == 42434.0);
  return 0;
}

static int test_invalid_v3_does_not_change_candidate(void) {
  static const char invalid[] =
    "version=3\nruleCount=2\nrule0.enabled=2\n";
  RuleFileV3 before = {0};
  RuleFileV3 candidate;

  before.slots[0].enabled = true;
  before.slots[0].threshold = 7u;
  candidate = before;
  ASSERT_TRUE(!rule_file_parse_v3((const uint8_t *)invalid, strlen(invalid), &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

static const char default_v4_text[] =
  "version=4\nruleCount=2\n"
  "rule0.enabled=1\nrule0.relay=0\nrule0.signalKey=EngineData.rpm\nrule0.threshold=1234.5\n"
  "rule0.action=on\nrule0.delayMs=1000\nrule0.timeoutMs=1500\nrule0.safeState=off\nrule0.priority=10\n"
  "rule1.enabled=1\nrule1.relay=1\nrule1.signalKey=VehicleData.speed\nrule1.threshold=-1.25\n"
  "rule1.action=off\nrule1.delayMs=0\nrule1.timeoutMs=1500\nrule1.safeState=off\nrule1.priority=20\n";

static int test_valid_v4_decimal_round_trip(void) {
  RuleFileV4 rules;
  RuleFileV4 round_trip;
  RuleEngine engine;
  char text[RULE_FILE_V4_MAX_BYTES + 1u];
  size_t len;

  ASSERT_TRUE(rule_file_parse_v4((const uint8_t *)default_v4_text, strlen(default_v4_text), &rules));
  ASSERT_TRUE(strcmp(rules.slots[0].signal_key, "EngineData.rpm") == 0);
  ASSERT_TRUE(fabs(rules.slots[0].threshold - 1234.5) < 0.000001);
  ASSERT_TRUE(fabs(rules.slots[1].threshold + 1.25) < 0.000001);
  len = rule_file_format_v4(&rules, text, sizeof(text));
  ASSERT_TRUE(len > 0u && len <= RULE_FILE_V4_MAX_BYTES);
  ASSERT_TRUE(rule_file_parse_v4((const uint8_t *)text, len, &round_trip));
  ASSERT_TRUE(strcmp(round_trip.slots[1].signal_key, "VehicleData.speed") == 0);
  ASSERT_TRUE(fabs(round_trip.slots[0].threshold - rules.slots[0].threshold) < 0.000001);
  ASSERT_TRUE(rule_file_v4_build_engine(&rules, &engine));
  ASSERT_TRUE(engine.rule_count == 2u);
  ASSERT_TRUE(strcmp(engine.rules[1].signal_key, "VehicleData.speed") == 0);
  return 0;
}

static int test_invalid_v4_does_not_change_candidate(void) {
  static const char invalid[] =
    "version=4\nruleCount=2\nrule0.enabled=1\nrule0.relay=0\n"
    "rule0.signalKey=EngineData.rpm\nrule0.threshold=1.2.3\n";
  RuleFileV4 before = {0};
  RuleFileV4 candidate;

  (void)snprintf(before.slots[0].signal_key, sizeof(before.slots[0].signal_key), "keep.key");
  before.slots[0].threshold = 7.5;
  candidate = before;
  ASSERT_TRUE(!rule_file_parse_v4((const uint8_t *)invalid, strlen(invalid), &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_valid_file() == 0);
  ASSERT_TRUE(test_missing_required_field_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_invalid_threshold_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_invalid_timing_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_valid_v2_default() == 0);
  ASSERT_TRUE(test_invalid_v2_classes_do_not_change_candidate() == 0);
  ASSERT_TRUE(test_oversized_v2_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_valid_v3_and_enabled_engine() == 0);
  ASSERT_TRUE(test_invalid_v3_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_valid_v4_decimal_round_trip() == 0);
  ASSERT_TRUE(test_invalid_v4_does_not_change_candidate() == 0);
  return 0;
}
