#include "rule_file.h"

#include <float.h>
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

static size_t reference_format_decimal(double value, char *text,
                                       size_t capacity) {
  if (text == NULL || capacity == 0u || !isfinite(value)) {
    return 0u;
  }
  const int written = snprintf(text, capacity, "%.9f", value);
  if (written < 0 || (size_t)written >= capacity) {
    return 0u;
  }
  size_t used = (size_t)written;
  while (used > 0u && text[used - 1u] == '0') {
    --used;
  }
  if (used > 0u && text[used - 1u] == '.') {
    --used;
  }
  if (used == 0u || (used == 1u && text[0] == '-')) {
    text[0] = '0';
    used = 1u;
  }
  text[used] = '\0';
  return used;
}

static int test_decimal_formatter_matches_fixed_nine_contract(void) {
  static const double values[] = {
    0.0,
    -0.0,
    1234.5,
    -1.25,
    0.0009765625,
    0.0029296875,
    0.0000000004,
    -0.0000000004,
    1.9999999996,
    9999999999.999999999,
    100000000000000000000.0,
    -10000000000000000000.0,
    0x1p-1074,
    DBL_MAX,
    -DBL_MAX,
  };
  for (size_t i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
    char expected[384];
    char actual[384];
    const size_t expected_len =
      reference_format_decimal(values[i], expected, sizeof(expected));
    const size_t actual_len =
      rule_file_format_decimal(values[i], actual, sizeof(actual));
    ASSERT_TRUE(actual_len == expected_len);
    ASSERT_TRUE(actual_len == 0u || strcmp(actual, expected) == 0);
  }
  uint64_t state = UINT64_C(0x4d595df4d0f33173);
  for (size_t i = 0u; i < 2048u; ++i) {
    char expected[384];
    char actual[384];
    double value;
    state = state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    if (((state >> 52u) & UINT64_C(0x7ff)) == UINT64_C(0x7ff)) {
      state ^= UINT64_C(0x0010000000000000);
    }
    memcpy(&value, &state, sizeof(value));
    const size_t expected_len =
      reference_format_decimal(value, expected, sizeof(expected));
    const size_t actual_len =
      rule_file_format_decimal(value, actual, sizeof(actual));
    ASSERT_TRUE(actual_len == expected_len);
    ASSERT_TRUE(actual_len == 0u || strcmp(actual, expected) == 0);
  }
  return 0;
}

static int test_decimal_formatter_capacity_and_invalid_values(void) {
  char text[32];
  ASSERT_TRUE(rule_file_format_decimal(1234.5, text, 14u) == 0u);
  ASSERT_TRUE(rule_file_format_decimal(1234.5, text, 15u) == 6u);
  ASSERT_TRUE(strcmp(text, "1234.5") == 0);
  ASSERT_TRUE(rule_file_format_decimal(100000000000000000000.0,
                                       text, sizeof(text)) == 21u);
  ASSERT_TRUE(strcmp(text, "100000000000000000000") == 0);
  ASSERT_TRUE(rule_file_format_decimal(1000000000000000000000.0,
                                       text, sizeof(text)) == 0u);
  ASSERT_TRUE(rule_file_format_decimal(-100000000000000000000.0,
                                       text, sizeof(text)) == 0u);
  ASSERT_TRUE(rule_file_format_decimal(NAN, text, sizeof(text)) == 0u);
  ASSERT_TRUE(rule_file_format_decimal(INFINITY, text, sizeof(text)) == 0u);
  ASSERT_TRUE(rule_file_format_decimal(-INFINITY, text, sizeof(text)) == 0u);
  ASSERT_TRUE(rule_file_format_decimal(1.0, NULL, sizeof(text)) == 0u);
  ASSERT_TRUE(rule_file_format_decimal(1.0, text, 0u) == 0u);
  return 0;
}

static int test_decimal_formatter_round_trip_is_stable(void) {
  static const double values[] = {
    -0.0, -1234.567890123, -0.0009765625, 0.0,
    0.000000001, 1.234567891, 1234.5, 9999999999.25,
  };
  for (size_t i = 0u; i < sizeof(values) / sizeof(values[0]); ++i) {
    char first[32];
    char second[32];
    double parsed;
    const size_t first_len =
      rule_file_format_decimal(values[i], first, sizeof(first));
    ASSERT_TRUE(first_len > 0u);
    ASSERT_TRUE(rule_file_parse_decimal(first, first_len, &parsed));
    const size_t second_len =
      rule_file_format_decimal(parsed, second, sizeof(second));
    ASSERT_TRUE(second_len == first_len);
    ASSERT_TRUE(strcmp(first, second) == 0);
  }
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

static const char default_v5_text[] =
  "version=5\nruleCount=2\n"
  "rule0.enabled=1\nrule0.relay=0\nrule0.signalKey=EngineData.rpm\nrule0.threshold=1234.5\n"
  "rule0.action=on\nrule0.delayMs=1000\nrule0.timeoutMs=1500\nrule0.safeState=off\nrule0.priority=10\n"
  "rule0.definitionHash=0000000000000000\n"
  "rule1.enabled=0\nrule1.relay=1\nrule1.signalKey=VehicleData.speed\nrule1.threshold=-1.25\n"
  "rule1.action=off\nrule1.delayMs=0\nrule1.timeoutMs=1500\nrule1.safeState=off\nrule1.priority=20\n"
  "rule1.definitionHash=FFFFFFFFFFFFFFFF\n";

static int test_v5_round_trip_and_hash_boundaries(void) {
  RuleFileV5 rules;
  RuleFileV5 round_trip;
  char text[RULE_FILE_V5_MAX_BYTES + 1u];

  ASSERT_TRUE(rule_file_parse_v5((const uint8_t *)default_v5_text,
                                 strlen(default_v5_text), &rules));
  ASSERT_TRUE(rules.slots[0].definition_hash == UINT64_C(0));
  ASSERT_TRUE(rules.slots[1].definition_hash == UINT64_MAX);
  const size_t len = rule_file_format_v5(&rules, text, sizeof(text));
  ASSERT_TRUE(len > 0u && len <= RULE_FILE_V5_MAX_BYTES);
  ASSERT_TRUE(strstr(text, "rule0.definitionHash=0000000000000000\n") != NULL);
  ASSERT_TRUE(strstr(text, "rule1.definitionHash=FFFFFFFFFFFFFFFF\n") != NULL);
  ASSERT_TRUE(rule_file_parse_v5((const uint8_t *)text, len, &round_trip));
  ASSERT_TRUE(memcmp(&round_trip, &rules, sizeof(rules)) == 0);
  return 0;
}

static int assert_invalid_v5_does_not_change_candidate(const char *text) {
  RuleFileV5 before;
  RuleFileV5 candidate;

  memset(&before, 0xA5, sizeof(before));
  candidate = before;
  ASSERT_TRUE(!rule_file_parse_v5((const uint8_t *)text, strlen(text),
                                  &candidate));
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

static int test_v5_strict_rejections(void) {
  char duplicate[RULE_FILE_V5_MAX_BYTES + 1u];
  char missing[sizeof(default_v5_text)];
  char lowercase_hex[sizeof(default_v5_text)];
  char non_hex[sizeof(default_v5_text)];
  char short_hex[sizeof(default_v5_text)];
  char wrong_version[sizeof(default_v5_text)];
  const char missing_line[] = "rule0.definitionHash=0000000000000000\n";

  const int duplicate_len = snprintf(
    duplicate, sizeof(duplicate), "%srule0.definitionHash=0000000000000000\n",
    default_v5_text);
  ASSERT_TRUE(duplicate_len > 0 && (size_t)duplicate_len < sizeof(duplicate));

  memcpy(missing, default_v5_text, sizeof(default_v5_text));
  char *line = strstr(missing, missing_line);
  ASSERT_TRUE(line != NULL);
  memmove(line, line + strlen(missing_line),
          strlen(line + strlen(missing_line)) + 1u);

  memcpy(lowercase_hex, default_v5_text, sizeof(default_v5_text));
  char *hash = strstr(lowercase_hex, "FFFFFFFFFFFFFFFF");
  ASSERT_TRUE(hash != NULL);
  hash[0] = 'f';

  memcpy(non_hex, default_v5_text, sizeof(default_v5_text));
  hash = strstr(non_hex, "FFFFFFFFFFFFFFFF");
  ASSERT_TRUE(hash != NULL);
  hash[0] = 'G';

  memcpy(short_hex, default_v5_text, sizeof(default_v5_text));
  hash = strstr(short_hex, "FFFFFFFFFFFFFFFF");
  ASSERT_TRUE(hash != NULL);
  memmove(hash, hash + 1u, strlen(hash));

  memcpy(wrong_version, default_v5_text, sizeof(default_v5_text));
  wrong_version[strlen("version=")] = '4';

  ASSERT_TRUE(assert_invalid_v5_does_not_change_candidate(duplicate) == 0);
  ASSERT_TRUE(assert_invalid_v5_does_not_change_candidate(missing) == 0);
  ASSERT_TRUE(assert_invalid_v5_does_not_change_candidate(lowercase_hex) == 0);
  ASSERT_TRUE(assert_invalid_v5_does_not_change_candidate(non_hex) == 0);
  ASSERT_TRUE(assert_invalid_v5_does_not_change_candidate(short_hex) == 0);
  ASSERT_TRUE(assert_invalid_v5_does_not_change_candidate(wrong_version) == 0);
  ASSERT_TRUE(assert_invalid_v5_does_not_change_candidate(default_v4_text) == 0);
  return 0;
}

static int test_v4_enabled_migration_is_rejected(void) {
  RuleFileV4 v4;
  RuleFileV5 before;
  RuleFileV5 candidate;

  ASSERT_TRUE(rule_file_parse_v4((const uint8_t *)default_v4_text,
                                 strlen(default_v4_text), &v4));
  memset(&before, 0x5A, sizeof(before));
  candidate = before;
  ASSERT_TRUE(rule_file_v5_migrate_disabled_v4(&v4, &candidate) ==
              RULE_FILE_V5_MIGRATION_ENABLED_RULE_REQUIRES_DEFINITION_HASH);
  ASSERT_TRUE(memcmp(&candidate, &before, sizeof(candidate)) == 0);
  return 0;
}

static int test_v4_all_disabled_explicit_migration(void) {
  RuleFileV4 v4;
  RuleFileV5 v5;
  RuleFileV5 parsed;
  char text[RULE_FILE_V5_MAX_BYTES + 1u];

  ASSERT_TRUE(rule_file_parse_v4((const uint8_t *)default_v4_text,
                                 strlen(default_v4_text), &v4));
  v4.slots[0].enabled = false;
  v4.slots[1].enabled = false;
  ASSERT_TRUE(rule_file_v5_migrate_disabled_v4(&v4, &v5) ==
              RULE_FILE_V5_MIGRATION_OK);
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    ASSERT_TRUE(!v5.slots[slot].enabled);
    ASSERT_TRUE(v5.slots[slot].definition_hash == UINT64_C(0));
    ASSERT_TRUE(v5.slots[slot].relay == v4.slots[slot].relay);
    ASSERT_TRUE(strcmp(v5.slots[slot].signal_key,
                       v4.slots[slot].signal_key) == 0);
    ASSERT_TRUE(v5.slots[slot].threshold == v4.slots[slot].threshold);
    ASSERT_TRUE(v5.slots[slot].priority == v4.slots[slot].priority);
  }
  const size_t len = rule_file_format_v5(&v5, text, sizeof(text));
  ASSERT_TRUE(len > 0u);
  ASSERT_TRUE(rule_file_parse_v5((const uint8_t *)text, len, &parsed));
  ASSERT_TRUE(memcmp(&parsed, &v5, sizeof(v5)) == 0);
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
  ASSERT_TRUE(test_decimal_formatter_matches_fixed_nine_contract() == 0);
  ASSERT_TRUE(test_decimal_formatter_capacity_and_invalid_values() == 0);
  ASSERT_TRUE(test_decimal_formatter_round_trip_is_stable() == 0);
  ASSERT_TRUE(test_invalid_v4_does_not_change_candidate() == 0);
  ASSERT_TRUE(test_v5_round_trip_and_hash_boundaries() == 0);
  ASSERT_TRUE(test_v5_strict_rejections() == 0);
  ASSERT_TRUE(test_v4_enabled_migration_is_rejected() == 0);
  ASSERT_TRUE(test_v4_all_disabled_explicit_migration() == 0);
  return 0;
}
