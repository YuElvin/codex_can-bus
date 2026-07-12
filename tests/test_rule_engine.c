#include "rule_engine.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_EQ_STATE(expected, actual)                                                    \
  do {                                                                                       \
    if ((expected) != (actual)) {                                                            \
      printf("ASSERT_EQ_STATE failed at %s:%d: expected %d got %d\n",                        \
             __FILE__,                                                                       \
             __LINE__,                                                                       \
             (int)(expected),                                                                \
             (int)(actual));                                                                 \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static Rule rpm_rule(void) {
  Rule rule;
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.id, sizeof(rule.id), "rpm_hi");
  snprintf(rule.signal_key, sizeof(rule.signal_key), "EngineData.rpm");
  rule.enabled = true;
  rule.op = RULE_OP_GT;
  rule.threshold = 3000.0;
  rule.relay = 0;
  rule.action_state = RELAY_STATE_ON;
  rule.timeout_ms = 100u;
  rule.safe_state = RELAY_STATE_OFF;
  rule.default_state = RELAY_STATE_OFF;
  return rule;
}

static int test_basic_match_and_default(void) {
  RuleEngine engine;
  rule_engine_init(&engine);
  Rule rule = rpm_rule();
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule));

  SignalSnapshot signals[] = {
    {.key = "EngineData.rpm", .value = 2500.0, .updated_ms = 10u, .valid = true},
  };
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_ON, RELAY_STATE_ON};

  rule_engine_evaluate(&engine, signals, 1u, 20u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[1]);

  signals[0].value = 3500.0;
  rule_engine_evaluate(&engine, signals, 1u, 30u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[1]);
  return 0;
}

static int test_manual_override_has_priority(void) {
  RuleEngine engine;
  rule_engine_init(&engine);
  Rule rule = rpm_rule();
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule));

  const RelayState manual[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_ON};
  rule_engine_set_manual(&engine, true, manual);

  SignalSnapshot signals[] = {
    {.key = "EngineData.rpm", .value = 6000.0, .updated_ms = 10u, .valid = true},
  };
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};

  rule_engine_evaluate(&engine, signals, 1u, 20u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[1]);
  return 0;
}

static int test_timeout_uses_safe_state(void) {
  RuleEngine engine;
  rule_engine_init(&engine);
  Rule rule = rpm_rule();
  rule.safe_state = RELAY_STATE_ON;
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule));

  SignalSnapshot signals[] = {
    {.key = "EngineData.rpm", .value = 6000.0, .updated_ms = 0u, .valid = true},
  };
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};

  rule_engine_evaluate(&engine, signals, 1u, 101u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);
  return 0;
}

static int test_delay_requires_continuous_match(void) {
  RuleEngine engine;
  rule_engine_init(&engine);
  Rule rule = rpm_rule();
  rule.delay_ms = 50u;
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule));

  SignalSnapshot signals[] = {
    {.key = "EngineData.rpm", .value = 3500.0, .updated_ms = 0u, .valid = true},
  };
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};

  rule_engine_evaluate(&engine, signals, 1u, 10u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  rule_engine_evaluate(&engine, signals, 1u, 59u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  rule_engine_evaluate(&engine, signals, 1u, 60u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);

  signals[0].value = 2000.0;
  rule_engine_evaluate(&engine, signals, 1u, 70u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  return 0;
}

static int test_hysteresis_high_latches_between_thresholds(void) {
  RuleEngine engine;
  rule_engine_init(&engine);

  Rule rule = rpm_rule();
  rule.op = RULE_OP_HYSTERESIS_HIGH;
  rule.on_threshold = 3000.0;
  rule.off_threshold = 2500.0;
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule));

  SignalSnapshot signals[] = {
    {.key = "EngineData.rpm", .value = 2900.0, .updated_ms = 0u, .valid = true},
  };
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};

  rule_engine_evaluate(&engine, signals, 1u, 10u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);

  signals[0].value = 3100.0;
  rule_engine_evaluate(&engine, signals, 1u, 20u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);

  signals[0].value = 2600.0;
  rule_engine_evaluate(&engine, signals, 1u, 30u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);

  signals[0].value = 2400.0;
  rule_engine_evaluate(&engine, signals, 1u, 40u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  return 0;
}

static Rule v2_rule(uint32_t threshold,
                    RelayState action_state,
                    uint32_t delay_ms,
                    uint8_t priority) {
  Rule rule;
  memset(&rule, 0, sizeof(rule));
  snprintf(rule.id, sizeof(rule.id), "v2");
  snprintf(rule.signal_key, sizeof(rule.signal_key), "Can2Data.marker");
  rule.enabled = true;
  rule.op = RULE_OP_GE;
  rule.threshold = (double)threshold;
  rule.relay = 0u;
  rule.action_state = action_state;
  rule.delay_ms = delay_ms;
  rule.timeout_ms = 1500u;
  rule.safe_state = RELAY_STATE_OFF;
  rule.default_state = RELAY_STATE_OFF;
  rule.priority = priority;
  return rule;
}

static int test_priority_winner_delay_and_timeout(void) {
  RuleEngine engine;
  rule_engine_init(&engine);
  Rule rule0 = v2_rule(42434u, RELAY_STATE_ON, 1000u, 10u);
  Rule rule1 = v2_rule(42435u, RELAY_STATE_OFF, 0u, 20u);
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule0));
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule1));

  SignalSnapshot signal = {
    .key = "Can2Data.marker",
    .value = 42434.0,
    .updated_ms = 10u,
    .valid = true,
  };
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};

  rule_engine_evaluate(&engine, &signal, 1u, 10u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  ASSERT_TRUE(engine.winner_rule[0] == 0u);
  rule_engine_evaluate(&engine, &signal, 1u, 1010u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);
  ASSERT_TRUE(engine.winner_rule[0] == 0u);

  signal.value = 42435.0;
  signal.updated_ms = 1020u;
  rule_engine_evaluate(&engine, &signal, 1u, 1020u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_OFF, relays[0]);
  ASSERT_TRUE(engine.winner_rule[0] == 1u);

  engine.rules[1].safe_state = RELAY_STATE_ON;
  signal.updated_ms = 1020u;
  rule_engine_evaluate(&engine, &signal, 1u, 2521u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);
  ASSERT_TRUE(engine.winner_rule[0] == 1u);
  return 0;
}

static int test_v2_manual_override_has_priority(void) {
  RuleEngine engine;
  rule_engine_init(&engine);
  Rule rule0 = v2_rule(42434u, RELAY_STATE_OFF, 0u, 10u);
  Rule rule1 = v2_rule(42435u, RELAY_STATE_OFF, 0u, 20u);
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule0));
  ASSERT_TRUE(rule_engine_add_rule(&engine, &rule1));

  const RelayState manual[RULE_RELAY_COUNT] = {RELAY_STATE_ON, RELAY_STATE_OFF};
  rule_engine_set_manual(&engine, true, manual);
  SignalSnapshot signal = {
    .key = "Can2Data.marker",
    .value = 42435.0,
    .updated_ms = 20u,
    .valid = true,
  };
  RelayState relays[RULE_RELAY_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};

  rule_engine_evaluate(&engine, &signal, 1u, 20u, relays);
  ASSERT_EQ_STATE(RELAY_STATE_ON, relays[0]);
  ASSERT_TRUE(engine.winner_rule[0] == UINT8_MAX);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_basic_match_and_default() == 0);
  ASSERT_TRUE(test_manual_override_has_priority() == 0);
  ASSERT_TRUE(test_timeout_uses_safe_state() == 0);
  ASSERT_TRUE(test_delay_requires_continuous_match() == 0);
  ASSERT_TRUE(test_hysteresis_high_latches_between_thresholds() == 0);
  ASSERT_TRUE(test_priority_winner_delay_and_timeout() == 0);
  ASSERT_TRUE(test_v2_manual_override_has_priority() == 0);
  return 0;
}
