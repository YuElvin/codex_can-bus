#include "rule_engine.h"

#include <math.h>
#include <string.h>

static const double rule_epsilon = 0.000001;

static const SignalSnapshot *find_signal(const SignalSnapshot *signals, size_t signal_count, const char *key) {
  if (signals == NULL || key == NULL) {
    return NULL;
  }

  for (size_t i = 0u; i < signal_count; ++i) {
    if (signals[i].valid && strcmp(signals[i].key, key) == 0) {
      return &signals[i];
    }
  }
  return NULL;
}

static bool is_timed_out(const SignalSnapshot *signal, uint32_t timeout_ms, uint32_t now_ms) {
  if (signal == NULL) {
    return true;
  }
  if (timeout_ms == 0u) {
    return false;
  }
  return now_ms - signal->updated_ms > timeout_ms;
}

static bool compare_basic(RuleOp op, double value, double threshold) {
  switch (op) {
    case RULE_OP_GT:
      return value > threshold;
    case RULE_OP_GE:
      return value >= threshold;
    case RULE_OP_LT:
      return value < threshold;
    case RULE_OP_LE:
      return value <= threshold;
    case RULE_OP_EQ:
      return fabs(value - threshold) <= rule_epsilon;
    case RULE_OP_NE:
      return fabs(value - threshold) > rule_epsilon;
    case RULE_OP_HYSTERESIS_HIGH:
    case RULE_OP_HYSTERESIS_LOW:
      return false;
  }
  return false;
}

static bool compare_rule(Rule *rule, double value) {
  if (rule->op == RULE_OP_HYSTERESIS_HIGH) {
    if (rule->latched_state) {
      rule->latched_state = value > rule->off_threshold;
    } else {
      rule->latched_state = value >= rule->on_threshold;
    }
    return rule->latched_state;
  }

  if (rule->op == RULE_OP_HYSTERESIS_LOW) {
    if (rule->latched_state) {
      rule->latched_state = value < rule->off_threshold;
    } else {
      rule->latched_state = value <= rule->on_threshold;
    }
    return rule->latched_state;
  }

  return compare_basic(rule->op, value, rule->threshold);
}

void rule_engine_init(RuleEngine *engine) {
  if (engine == NULL) {
    return;
  }

  memset(engine, 0, sizeof(*engine));
  for (size_t i = 0u; i < RULE_RELAY_COUNT; ++i) {
    engine->relay_defaults[i] = RELAY_STATE_OFF;
    engine->manual.relay[i] = RELAY_STATE_OFF;
    engine->winner_rule[i] = UINT8_MAX;
  }
}

bool rule_engine_add_rule(RuleEngine *engine, const Rule *rule) {
  if (engine == NULL || rule == NULL || engine->rule_count >= RULE_ENGINE_MAX_RULES) {
    return false;
  }
  if (rule->relay >= RULE_RELAY_COUNT || rule->signal_key[0] == '\0') {
    return false;
  }

  engine->rules[engine->rule_count] = *rule;
  ++engine->rule_count;
  return true;
}

void rule_engine_set_manual(RuleEngine *engine, bool enabled, const RelayState relay[RULE_RELAY_COUNT]) {
  if (engine == NULL) {
    return;
  }

  engine->manual.enabled = enabled;
  if (relay != NULL) {
    for (size_t i = 0u; i < RULE_RELAY_COUNT; ++i) {
      engine->manual.relay[i] = relay[i];
    }
  }
}

void rule_engine_evaluate(RuleEngine *engine,
                          const SignalSnapshot *signals,
                          size_t signal_count,
                          uint32_t now_ms,
                          RelayState out_relays[RULE_RELAY_COUNT]) {
  bool selected[RULE_RELAY_COUNT] = {false};
  uint8_t selected_priority[RULE_RELAY_COUNT] = {0u};

  if (engine == NULL || out_relays == NULL) {
    return;
  }

  for (size_t i = 0u; i < RULE_RELAY_COUNT; ++i) {
    engine->winner_rule[i] = UINT8_MAX;
  }

  if (engine->manual.enabled) {
    for (size_t i = 0u; i < RULE_RELAY_COUNT; ++i) {
      out_relays[i] = engine->manual.relay[i];
    }
    return;
  }

  for (size_t i = 0u; i < RULE_RELAY_COUNT; ++i) {
    out_relays[i] = engine->relay_defaults[i];
  }

  for (size_t i = 0u; i < engine->rule_count; ++i) {
    Rule *rule = &engine->rules[i];
    if (!rule->enabled || rule->relay >= RULE_RELAY_COUNT) {
      continue;
    }

    const SignalSnapshot *signal = find_signal(signals, signal_count, rule->signal_key);
    if (is_timed_out(signal, rule->timeout_ms, now_ms)) {
      rule->condition_since_ms = 0u;
      if (!selected[rule->relay] || rule->priority > selected_priority[rule->relay]) {
        selected[rule->relay] = true;
        selected_priority[rule->relay] = rule->priority;
        engine->winner_rule[rule->relay] = (uint8_t)i;
        out_relays[rule->relay] = rule->safe_state;
      }
      continue;
    }

    const bool matched = compare_rule(rule, signal->value);
    if (!matched) {
      rule->condition_since_ms = 0u;
      continue;
    }

    if (rule->delay_ms == 0u) {
      if (!selected[rule->relay] || rule->priority > selected_priority[rule->relay]) {
        selected[rule->relay] = true;
        selected_priority[rule->relay] = rule->priority;
        engine->winner_rule[rule->relay] = (uint8_t)i;
        out_relays[rule->relay] = rule->action_state;
      }
      continue;
    }

    if (rule->condition_since_ms == 0u) {
      rule->condition_since_ms = now_ms;
    }
    if (!selected[rule->relay] || rule->priority > selected_priority[rule->relay]) {
      selected[rule->relay] = true;
      selected_priority[rule->relay] = rule->priority;
      engine->winner_rule[rule->relay] = (uint8_t)i;
      if (now_ms - rule->condition_since_ms >= rule->delay_ms) {
        out_relays[rule->relay] = rule->action_state;
      } else {
        out_relays[rule->relay] = rule->default_state;
      }
    }
  }
}
