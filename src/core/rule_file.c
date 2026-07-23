#include "rule_file.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
  RULE_FILE_FIELD_VERSION = 1u << 0,
  RULE_FILE_FIELD_ON_THRESHOLD = 1u << 1,
  RULE_FILE_FIELD_OFF_THRESHOLD = 1u << 2,
  RULE_FILE_FIELD_DELAY_MS = 1u << 3,
  RULE_FILE_FIELD_TIMEOUT_MS = 1u << 4,
  RULE_FILE_FIELD_ALL = RULE_FILE_FIELD_VERSION |
                        RULE_FILE_FIELD_ON_THRESHOLD |
                        RULE_FILE_FIELD_OFF_THRESHOLD |
                        RULE_FILE_FIELD_DELAY_MS |
                        RULE_FILE_FIELD_TIMEOUT_MS,
};

static bool rule_file_key_is(const uint8_t *key, size_t key_len, const char *expected) {
  return strlen(expected) == key_len && memcmp(key, expected, key_len) == 0;
}

static bool rule_file_parse_u32(const uint8_t *text, size_t len, uint32_t *value) {
  uint32_t result = 0u;

  if (text == NULL || value == NULL || len == 0u) {
    return false;
  }
  for (size_t i = 0u; i < len; ++i) {
    if (text[i] < (uint8_t)'0' || text[i] > (uint8_t)'9') {
      return false;
    }
    const uint32_t digit = (uint32_t)(text[i] - (uint8_t)'0');
    if (result > (UINT32_MAX - digit) / 10u) {
      return false;
    }
    result = result * 10u + digit;
  }
  *value = result;
  return true;
}

bool rule_file_parse_v1(const uint8_t *data, size_t len, RuleTaskConfig *out_config) {
  RuleTaskConfig parsed = {0};
  uint32_t version = 0u;
  uint32_t on_threshold = 0u;
  uint32_t off_threshold = 0u;
  uint32_t delay_ms = 0u;
  uint32_t timeout_ms = 0u;
  uint32_t fields = 0u;
  size_t offset = 0u;

  if (data == NULL || out_config == NULL || len == 0u || len > RULE_FILE_V1_MAX_BYTES) {
    return false;
  }

  while (offset < len) {
    const size_t line_start = offset;
    size_t line_len;
    size_t equals = 0u;
    bool has_equals = false;

    while (offset < len && data[offset] != (uint8_t)'\n') {
      ++offset;
    }
    line_len = offset - line_start;
    if (offset < len) {
      ++offset;
    }
    if (line_len > 0u && data[line_start + line_len - 1u] == (uint8_t)'\r') {
      --line_len;
    }
    if (line_len == 0u) {
      continue;
    }

    for (size_t i = 0u; i < line_len; ++i) {
      if (data[line_start + i] == (uint8_t)'=') {
        if (has_equals) {
          return false;
        }
        equals = i;
        has_equals = true;
      }
    }
    if (!has_equals || equals == 0u || equals + 1u >= line_len) {
      return false;
    }

    uint32_t value = 0u;
    uint32_t field = 0u;
    const uint8_t *key = data + line_start;
    const uint8_t *value_text = data + line_start + equals + 1u;
    const size_t value_len = line_len - equals - 1u;

    if (rule_file_key_is(key, equals, "version")) {
      field = RULE_FILE_FIELD_VERSION;
    } else if (rule_file_key_is(key, equals, "onThreshold")) {
      field = RULE_FILE_FIELD_ON_THRESHOLD;
    } else if (rule_file_key_is(key, equals, "offThreshold")) {
      field = RULE_FILE_FIELD_OFF_THRESHOLD;
    } else if (rule_file_key_is(key, equals, "delayMs")) {
      field = RULE_FILE_FIELD_DELAY_MS;
    } else if (rule_file_key_is(key, equals, "timeoutMs")) {
      field = RULE_FILE_FIELD_TIMEOUT_MS;
    } else {
      return false;
    }
    if ((fields & field) != 0u || !rule_file_parse_u32(value_text, value_len, &value)) {
      return false;
    }
    fields |= field;
    switch (field) {
      case RULE_FILE_FIELD_VERSION:
        version = value;
        break;
      case RULE_FILE_FIELD_ON_THRESHOLD:
        on_threshold = value;
        break;
      case RULE_FILE_FIELD_OFF_THRESHOLD:
        off_threshold = value;
        break;
      case RULE_FILE_FIELD_DELAY_MS:
        delay_ms = value;
        break;
      case RULE_FILE_FIELD_TIMEOUT_MS:
        timeout_ms = value;
        break;
      default:
        return false;
    }
  }

  if (fields != RULE_FILE_FIELD_ALL || version != 1u || on_threshold <= off_threshold ||
      delay_ms > timeout_ms) {
    return false;
  }

  parsed.on_threshold = (double)on_threshold;
  parsed.off_threshold = (double)off_threshold;
  parsed.delay_ms = delay_ms;
  parsed.timeout_ms = timeout_ms;
  *out_config = parsed;
  return true;
}

enum {
  RULE_FILE_V2_FIELD_VERSION = 1u << 0,
  RULE_FILE_V2_FIELD_COUNT = 1u << 1,
  RULE_FILE_V2_RULE_FIELD_RELAY = 1u << 0,
  RULE_FILE_V2_RULE_FIELD_THRESHOLD = 1u << 1,
  RULE_FILE_V2_RULE_FIELD_ACTION = 1u << 2,
  RULE_FILE_V2_RULE_FIELD_DELAY = 1u << 3,
  RULE_FILE_V2_RULE_FIELD_TIMEOUT = 1u << 4,
  RULE_FILE_V2_RULE_FIELD_SAFE_STATE = 1u << 5,
  RULE_FILE_V2_RULE_FIELD_PRIORITY = 1u << 6,
  RULE_FILE_V2_RULE_FIELD_ALL = RULE_FILE_V2_RULE_FIELD_RELAY |
                                RULE_FILE_V2_RULE_FIELD_THRESHOLD |
                                RULE_FILE_V2_RULE_FIELD_ACTION |
                                RULE_FILE_V2_RULE_FIELD_DELAY |
                                RULE_FILE_V2_RULE_FIELD_TIMEOUT |
                                RULE_FILE_V2_RULE_FIELD_SAFE_STATE |
                                RULE_FILE_V2_RULE_FIELD_PRIORITY,
};

static bool rule_file_v2_key_is(const uint8_t *key,
                                size_t key_len,
                                size_t rule_index,
                                const char *field_name) {
  const size_t field_len = strlen(field_name);

  return key != NULL && rule_index < RULE_FILE_V2_RULE_COUNT && key_len == 6u + field_len &&
         key[0] == (uint8_t)'r' && key[1] == (uint8_t)'u' && key[2] == (uint8_t)'l' &&
         key[3] == (uint8_t)'e' && key[4] == (uint8_t)('0' + rule_index) && key[5] == (uint8_t)'.' &&
         memcmp(key + 6u, field_name, field_len) == 0;
}

static bool rule_file_v2_parse_state(const uint8_t *text, size_t len, RelayState *state) {
  if (text == NULL || state == NULL) {
    return false;
  }
  if (rule_file_key_is(text, len, "on")) {
    *state = RELAY_STATE_ON;
    return true;
  }
  if (rule_file_key_is(text, len, "off")) {
    *state = RELAY_STATE_OFF;
    return true;
  }
  return false;
}

bool rule_file_parse_v2(const uint8_t *data, size_t len, RuleEngine *out_engine) {
  RuleEngine parsed_engine;
  uint32_t version = 0u;
  uint32_t rule_count = 0u;
  uint32_t global_fields = 0u;
  uint32_t rule_fields[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t relay[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t threshold[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t delay_ms[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t timeout_ms[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t priority[RULE_FILE_V2_RULE_COUNT] = {0u};
  RelayState action[RULE_FILE_V2_RULE_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};
  RelayState safe_state[RULE_FILE_V2_RULE_COUNT] = {RELAY_STATE_OFF, RELAY_STATE_OFF};
  size_t offset = 0u;

  if (data == NULL || out_engine == NULL || len == 0u || len > RULE_FILE_V2_MAX_BYTES) {
    return false;
  }

  while (offset < len) {
    const size_t line_start = offset;
    size_t line_len;
    size_t equals = 0u;
    bool has_equals = false;

    while (offset < len && data[offset] != (uint8_t)'\n') {
      ++offset;
    }
    line_len = offset - line_start;
    if (offset < len) {
      ++offset;
    }
    if (line_len > 0u && data[line_start + line_len - 1u] == (uint8_t)'\r') {
      --line_len;
    }
    if (line_len == 0u) {
      continue;
    }

    for (size_t i = 0u; i < line_len; ++i) {
      if (data[line_start + i] == (uint8_t)'=') {
        if (has_equals) {
          return false;
        }
        equals = i;
        has_equals = true;
      }
    }
    if (!has_equals || equals == 0u || equals + 1u >= line_len) {
      return false;
    }

    const uint8_t *key = data + line_start;
    const size_t key_len = equals;
    const uint8_t *value_text = data + line_start + equals + 1u;
    const size_t value_len = line_len - equals - 1u;
    uint32_t value = 0u;
    bool recognized = false;

    if (rule_file_key_is(key, key_len, "version")) {
      if ((global_fields & RULE_FILE_V2_FIELD_VERSION) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &value)) {
        return false;
      }
      global_fields |= RULE_FILE_V2_FIELD_VERSION;
      version = value;
      recognized = true;
    } else if (rule_file_key_is(key, key_len, "ruleCount")) {
      if ((global_fields & RULE_FILE_V2_FIELD_COUNT) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &value)) {
        return false;
      }
      global_fields |= RULE_FILE_V2_FIELD_COUNT;
      rule_count = value;
      recognized = true;
    }

    if (recognized) {
      continue;
    }

    for (size_t rule_index = 0u; rule_index < RULE_FILE_V2_RULE_COUNT; ++rule_index) {
      uint32_t field = 0u;
      if (rule_file_v2_key_is(key, key_len, rule_index, "relay")) {
        field = RULE_FILE_V2_RULE_FIELD_RELAY;
      } else if (rule_file_v2_key_is(key, key_len, rule_index, "threshold")) {
        field = RULE_FILE_V2_RULE_FIELD_THRESHOLD;
      } else if (rule_file_v2_key_is(key, key_len, rule_index, "action")) {
        field = RULE_FILE_V2_RULE_FIELD_ACTION;
      } else if (rule_file_v2_key_is(key, key_len, rule_index, "delayMs")) {
        field = RULE_FILE_V2_RULE_FIELD_DELAY;
      } else if (rule_file_v2_key_is(key, key_len, rule_index, "timeoutMs")) {
        field = RULE_FILE_V2_RULE_FIELD_TIMEOUT;
      } else if (rule_file_v2_key_is(key, key_len, rule_index, "safeState")) {
        field = RULE_FILE_V2_RULE_FIELD_SAFE_STATE;
      } else if (rule_file_v2_key_is(key, key_len, rule_index, "priority")) {
        field = RULE_FILE_V2_RULE_FIELD_PRIORITY;
      }
      if (field == 0u) {
        continue;
      }
      if ((rule_fields[rule_index] & field) != 0u) {
        return false;
      }
      if (field == RULE_FILE_V2_RULE_FIELD_ACTION || field == RULE_FILE_V2_RULE_FIELD_SAFE_STATE) {
        RelayState *state = field == RULE_FILE_V2_RULE_FIELD_ACTION ? &action[rule_index] : &safe_state[rule_index];
        if (!rule_file_v2_parse_state(value_text, value_len, state)) {
          return false;
        }
      } else if (!rule_file_parse_u32(value_text, value_len, &value)) {
        return false;
      }
      if (field == RULE_FILE_V2_RULE_FIELD_RELAY) {
        if (value > 1u) {
          return false;
        }
        relay[rule_index] = value;
      } else if (field == RULE_FILE_V2_RULE_FIELD_THRESHOLD) {
        threshold[rule_index] = value;
      } else if (field == RULE_FILE_V2_RULE_FIELD_DELAY) {
        delay_ms[rule_index] = value;
      } else if (field == RULE_FILE_V2_RULE_FIELD_TIMEOUT) {
        timeout_ms[rule_index] = value;
      } else if (field == RULE_FILE_V2_RULE_FIELD_PRIORITY) {
        if (value > UINT8_MAX) {
          return false;
        }
        priority[rule_index] = value;
      }
      rule_fields[rule_index] |= field;
      recognized = true;
      break;
    }
    if (!recognized) {
      return false;
    }
  }

  if (global_fields != (RULE_FILE_V2_FIELD_VERSION | RULE_FILE_V2_FIELD_COUNT) || version != 2u ||
      rule_count != RULE_FILE_V2_RULE_COUNT) {
    return false;
  }
  for (size_t i = 0u; i < RULE_FILE_V2_RULE_COUNT; ++i) {
    if (rule_fields[i] != RULE_FILE_V2_RULE_FIELD_ALL || delay_ms[i] > timeout_ms[i]) {
      return false;
    }
  }
  if (priority[0] == priority[1]) {
    return false;
  }

  rule_engine_init(&parsed_engine);
  for (size_t i = 0u; i < RULE_FILE_V2_RULE_COUNT; ++i) {
    Rule rule;
    memset(&rule, 0, sizeof(rule));
    (void)snprintf(rule.id, sizeof(rule.id), "rule%u", (unsigned)i);
    (void)snprintf(rule.signal_key, sizeof(rule.signal_key), "Can2Data.marker");
    rule.enabled = true;
    rule.op = RULE_OP_GE;
    rule.threshold = (double)threshold[i];
    rule.relay = (uint8_t)relay[i];
    rule.action_state = action[i];
    rule.delay_ms = delay_ms[i];
    rule.timeout_ms = timeout_ms[i];
    rule.safe_state = safe_state[i];
    rule.default_state = RELAY_STATE_OFF;
    rule.priority = (uint8_t)priority[i];
    if (!rule_engine_add_rule(&parsed_engine, &rule)) {
      return false;
    }
  }
  *out_engine = parsed_engine;
  return true;
}

enum {
  RULE_FILE_V3_FIELD_VERSION = 1u << 0,
  RULE_FILE_V3_FIELD_COUNT = 1u << 1,
  RULE_FILE_V3_SLOT_FIELD_ENABLED = 1u << 0,
  RULE_FILE_V3_SLOT_FIELD_RELAY = 1u << 1,
  RULE_FILE_V3_SLOT_FIELD_THRESHOLD = 1u << 2,
  RULE_FILE_V3_SLOT_FIELD_ACTION = 1u << 3,
  RULE_FILE_V3_SLOT_FIELD_DELAY = 1u << 4,
  RULE_FILE_V3_SLOT_FIELD_TIMEOUT = 1u << 5,
  RULE_FILE_V3_SLOT_FIELD_SAFE_STATE = 1u << 6,
  RULE_FILE_V3_SLOT_FIELD_PRIORITY = 1u << 7,
  RULE_FILE_V3_SLOT_FIELD_ALL = RULE_FILE_V3_SLOT_FIELD_ENABLED |
                                RULE_FILE_V3_SLOT_FIELD_RELAY |
                                RULE_FILE_V3_SLOT_FIELD_THRESHOLD |
                                RULE_FILE_V3_SLOT_FIELD_ACTION |
                                RULE_FILE_V3_SLOT_FIELD_DELAY |
                                RULE_FILE_V3_SLOT_FIELD_TIMEOUT |
                                RULE_FILE_V3_SLOT_FIELD_SAFE_STATE |
                                RULE_FILE_V3_SLOT_FIELD_PRIORITY,
};

bool rule_file_parse_v3(const uint8_t *data, size_t len, RuleFileV3 *out_rules) {
  RuleFileV3 parsed = {0};
  uint32_t global_fields = 0u;
  uint32_t slot_fields[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t version = 0u;
  uint32_t count = 0u;
  size_t offset = 0u;

  if (data == NULL || out_rules == NULL || len == 0u || len > RULE_FILE_V3_MAX_BYTES) {
    return false;
  }
  while (offset < len) {
    const size_t line_start = offset;
    size_t line_len;
    size_t equals = 0u;
    bool has_equals = false;
    while (offset < len && data[offset] != (uint8_t)'\n') {
      ++offset;
    }
    line_len = offset - line_start;
    if (offset < len) {
      ++offset;
    }
    if (line_len > 0u && data[line_start + line_len - 1u] == (uint8_t)'\r') {
      --line_len;
    }
    if (line_len == 0u) {
      continue;
    }
    for (size_t i = 0u; i < line_len; ++i) {
      if (data[line_start + i] == (uint8_t)'=') {
        if (has_equals) {
          return false;
        }
        equals = i;
        has_equals = true;
      }
    }
    if (!has_equals || equals == 0u || equals + 1u >= line_len) {
      return false;
    }
    const uint8_t *key = data + line_start;
    const uint8_t *value_text = key + equals + 1u;
    const size_t value_len = line_len - equals - 1u;
    uint32_t value = 0u;
    if (rule_file_key_is(key, equals, "version")) {
      if ((global_fields & RULE_FILE_V3_FIELD_VERSION) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &version)) return false;
      global_fields |= RULE_FILE_V3_FIELD_VERSION;
      continue;
    }
    if (rule_file_key_is(key, equals, "ruleCount")) {
      if ((global_fields & RULE_FILE_V3_FIELD_COUNT) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &count)) return false;
      global_fields |= RULE_FILE_V3_FIELD_COUNT;
      continue;
    }
    bool recognized = false;
    for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
      uint32_t field = 0u;
      if (rule_file_v2_key_is(key, equals, slot, "enabled")) field = RULE_FILE_V3_SLOT_FIELD_ENABLED;
      else if (rule_file_v2_key_is(key, equals, slot, "relay")) field = RULE_FILE_V3_SLOT_FIELD_RELAY;
      else if (rule_file_v2_key_is(key, equals, slot, "threshold")) field = RULE_FILE_V3_SLOT_FIELD_THRESHOLD;
      else if (rule_file_v2_key_is(key, equals, slot, "action")) field = RULE_FILE_V3_SLOT_FIELD_ACTION;
      else if (rule_file_v2_key_is(key, equals, slot, "delayMs")) field = RULE_FILE_V3_SLOT_FIELD_DELAY;
      else if (rule_file_v2_key_is(key, equals, slot, "timeoutMs")) field = RULE_FILE_V3_SLOT_FIELD_TIMEOUT;
      else if (rule_file_v2_key_is(key, equals, slot, "safeState")) field = RULE_FILE_V3_SLOT_FIELD_SAFE_STATE;
      else if (rule_file_v2_key_is(key, equals, slot, "priority")) field = RULE_FILE_V3_SLOT_FIELD_PRIORITY;
      if (field == 0u) continue;
      if ((slot_fields[slot] & field) != 0u) return false;
      if (field == RULE_FILE_V3_SLOT_FIELD_ACTION || field == RULE_FILE_V3_SLOT_FIELD_SAFE_STATE) {
        RelayState *state = field == RULE_FILE_V3_SLOT_FIELD_ACTION ?
          &parsed.slots[slot].action_state : &parsed.slots[slot].safe_state;
        if (!rule_file_v2_parse_state(value_text, value_len, state)) return false;
      } else if (!rule_file_parse_u32(value_text, value_len, &value)) return false;
      if (field == RULE_FILE_V3_SLOT_FIELD_ENABLED) {
        if (value > 1u) return false;
        parsed.slots[slot].enabled = value != 0u;
      } else if (field == RULE_FILE_V3_SLOT_FIELD_RELAY) {
        if (value >= RULE_RELAY_COUNT) return false;
        parsed.slots[slot].relay = (uint8_t)value;
      } else if (field == RULE_FILE_V3_SLOT_FIELD_THRESHOLD) parsed.slots[slot].threshold = value;
      else if (field == RULE_FILE_V3_SLOT_FIELD_DELAY) parsed.slots[slot].delay_ms = value;
      else if (field == RULE_FILE_V3_SLOT_FIELD_TIMEOUT) parsed.slots[slot].timeout_ms = value;
      else if (field == RULE_FILE_V3_SLOT_FIELD_PRIORITY) {
        if (value > UINT8_MAX) return false;
        parsed.slots[slot].priority = (uint8_t)value;
      }
      slot_fields[slot] |= field;
      recognized = true;
      break;
    }
    if (!recognized) return false;
  }
  if (global_fields != (RULE_FILE_V3_FIELD_VERSION | RULE_FILE_V3_FIELD_COUNT) || version != 3u ||
      count != RULE_FILE_V2_RULE_COUNT || parsed.slots[0].priority == parsed.slots[1].priority) return false;
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    if (slot_fields[slot] != RULE_FILE_V3_SLOT_FIELD_ALL ||
        parsed.slots[slot].delay_ms > parsed.slots[slot].timeout_ms) return false;
  }
  *out_rules = parsed;
  return true;
}

bool rule_file_v3_build_engine(const RuleFileV3 *rules, RuleEngine *out_engine) {
  if (rules == NULL || out_engine == NULL) return false;
  rule_engine_init(out_engine);
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    if (!rules->slots[slot].enabled) continue;
    Rule rule = {0};
    const RuleFileV3Slot *source = &rules->slots[slot];
    (void)snprintf(rule.id, sizeof(rule.id), "rule%u", (unsigned)slot);
    (void)snprintf(rule.signal_key, sizeof(rule.signal_key), "Can2Data.marker");
    rule.enabled = true;
    rule.op = RULE_OP_GE;
    rule.threshold = (double)source->threshold;
    rule.relay = source->relay;
    rule.action_state = source->action_state;
    rule.delay_ms = source->delay_ms;
    rule.timeout_ms = source->timeout_ms;
    rule.safe_state = source->safe_state;
    rule.default_state = RELAY_STATE_OFF;
    rule.priority = source->priority;
    if (!rule_engine_add_rule(out_engine, &rule)) return false;
  }
  return true;
}

size_t rule_file_format_v3(const RuleFileV3 *rules, char *out_text, size_t out_capacity) {
  size_t used = 0u;

  if (rules == NULL || out_text == NULL || out_capacity == 0u) return 0u;
  const int header = snprintf(out_text, out_capacity, "version=3\nruleCount=2\n");
  if (header < 0 || (size_t)header >= out_capacity) return 0u;
  used = (size_t)header;
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    const RuleFileV3Slot *rule = &rules->slots[slot];
    const int written = snprintf(out_text + used, out_capacity - used,
                                 "rule%u.enabled=%u\nrule%u.relay=%u\nrule%u.threshold=%lu\n"
                                 "rule%u.action=%s\nrule%u.delayMs=%lu\nrule%u.timeoutMs=%lu\n"
                                 "rule%u.safeState=%s\nrule%u.priority=%u\n",
                                 (unsigned)slot, rule->enabled ? 1u : 0u,
                                 (unsigned)slot, (unsigned)rule->relay,
                                 (unsigned)slot, (unsigned long)rule->threshold,
                                 (unsigned)slot, rule->action_state == RELAY_STATE_ON ? "on" : "off",
                                 (unsigned)slot, (unsigned long)rule->delay_ms,
                                 (unsigned)slot, (unsigned long)rule->timeout_ms,
                                 (unsigned)slot, rule->safe_state == RELAY_STATE_ON ? "on" : "off",
                                 (unsigned)slot, (unsigned)rule->priority);
    if (written < 0 || (size_t)written >= out_capacity - used) return 0u;
    used += (size_t)written;
  }
  return used;
}

bool rule_file_parse_decimal(const char *text, size_t len, double *value) {
  double parsed = 0.0;
  double place = 0.1;
  bool negative = false;
  bool has_digit = false;
  size_t offset = 0u;

  if (text == NULL || value == NULL || len == 0u) {
    return false;
  }
  if (text[offset] == '+' || text[offset] == '-') {
    negative = text[offset] == '-';
    ++offset;
  }
  while (offset < len && text[offset] >= '0' && text[offset] <= '9') {
    parsed = parsed * 10.0 + (double)(text[offset] - '0');
    has_digit = true;
    ++offset;
  }
  if (offset < len && text[offset] == '.') {
    ++offset;
    while (offset < len && text[offset] >= '0' && text[offset] <= '9') {
      parsed += (double)(text[offset] - '0') * place;
      place *= 0.1;
      has_digit = true;
      ++offset;
    }
  }
  if (!has_digit || offset != len || !isfinite(parsed)) {
    return false;
  }
  *value = negative ? -parsed : parsed;
  return true;
}

size_t rule_file_format_decimal(double value, char *out_text, size_t out_capacity) {
  int written;
  size_t used;

  if (out_text == NULL || out_capacity == 0u || !isfinite(value)) {
    return 0u;
  }
  written = snprintf(out_text, out_capacity, "%.9f", value);
  if (written < 0 || (size_t)written >= out_capacity) {
    return 0u;
  }
  used = (size_t)written;
  while (used > 0u && out_text[used - 1u] == '0') {
    --used;
  }
  if (used > 0u && out_text[used - 1u] == '.') {
    --used;
  }
  if (used == 0u || (used == 1u && out_text[0] == '-')) {
    out_text[0] = '0';
    used = 1u;
  }
  out_text[used] = '\0';
  return used;
}

enum {
  RULE_FILE_V4_FIELD_VERSION = 1u << 0,
  RULE_FILE_V4_FIELD_COUNT = 1u << 1,
  RULE_FILE_V4_SLOT_FIELD_ENABLED = 1u << 0,
  RULE_FILE_V4_SLOT_FIELD_RELAY = 1u << 1,
  RULE_FILE_V4_SLOT_FIELD_SIGNAL_KEY = 1u << 2,
  RULE_FILE_V4_SLOT_FIELD_THRESHOLD = 1u << 3,
  RULE_FILE_V4_SLOT_FIELD_ACTION = 1u << 4,
  RULE_FILE_V4_SLOT_FIELD_DELAY = 1u << 5,
  RULE_FILE_V4_SLOT_FIELD_TIMEOUT = 1u << 6,
  RULE_FILE_V4_SLOT_FIELD_SAFE_STATE = 1u << 7,
  RULE_FILE_V4_SLOT_FIELD_PRIORITY = 1u << 8,
  RULE_FILE_V4_SLOT_FIELD_ALL = RULE_FILE_V4_SLOT_FIELD_ENABLED |
                                RULE_FILE_V4_SLOT_FIELD_RELAY |
                                RULE_FILE_V4_SLOT_FIELD_SIGNAL_KEY |
                                RULE_FILE_V4_SLOT_FIELD_THRESHOLD |
                                RULE_FILE_V4_SLOT_FIELD_ACTION |
                                RULE_FILE_V4_SLOT_FIELD_DELAY |
                                RULE_FILE_V4_SLOT_FIELD_TIMEOUT |
                                RULE_FILE_V4_SLOT_FIELD_SAFE_STATE |
                                RULE_FILE_V4_SLOT_FIELD_PRIORITY,
};

bool rule_file_parse_v4(const uint8_t *data, size_t len, RuleFileV4 *out_rules) {
  RuleFileV4 parsed = {0};
  uint32_t global_fields = 0u;
  uint32_t slot_fields[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t version = 0u;
  uint32_t count = 0u;
  size_t offset = 0u;

  if (data == NULL || out_rules == NULL || len == 0u || len > RULE_FILE_V4_MAX_BYTES) {
    return false;
  }
  while (offset < len) {
    const size_t line_start = offset;
    size_t line_len;
    size_t equals = 0u;
    bool has_equals = false;
    while (offset < len && data[offset] != (uint8_t)'\n') {
      ++offset;
    }
    line_len = offset - line_start;
    if (offset < len) {
      ++offset;
    }
    if (line_len > 0u && data[line_start + line_len - 1u] == (uint8_t)'\r') {
      --line_len;
    }
    if (line_len == 0u) {
      continue;
    }
    for (size_t i = 0u; i < line_len; ++i) {
      if (data[line_start + i] == (uint8_t)'=') {
        if (has_equals) {
          return false;
        }
        equals = i;
        has_equals = true;
      }
    }
    if (!has_equals || equals == 0u || equals + 1u >= line_len) {
      return false;
    }
    const uint8_t *key = data + line_start;
    const uint8_t *value_text = key + equals + 1u;
    const size_t value_len = line_len - equals - 1u;
    uint32_t value = 0u;
    if (rule_file_key_is(key, equals, "version")) {
      if ((global_fields & RULE_FILE_V4_FIELD_VERSION) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &version)) {
        return false;
      }
      global_fields |= RULE_FILE_V4_FIELD_VERSION;
      continue;
    }
    if (rule_file_key_is(key, equals, "ruleCount")) {
      if ((global_fields & RULE_FILE_V4_FIELD_COUNT) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &count)) {
        return false;
      }
      global_fields |= RULE_FILE_V4_FIELD_COUNT;
      continue;
    }
    bool recognized = false;
    for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
      uint32_t field = 0u;
      if (rule_file_v2_key_is(key, equals, slot, "enabled")) field = RULE_FILE_V4_SLOT_FIELD_ENABLED;
      else if (rule_file_v2_key_is(key, equals, slot, "relay")) field = RULE_FILE_V4_SLOT_FIELD_RELAY;
      else if (rule_file_v2_key_is(key, equals, slot, "signalKey")) field = RULE_FILE_V4_SLOT_FIELD_SIGNAL_KEY;
      else if (rule_file_v2_key_is(key, equals, slot, "threshold")) field = RULE_FILE_V4_SLOT_FIELD_THRESHOLD;
      else if (rule_file_v2_key_is(key, equals, slot, "action")) field = RULE_FILE_V4_SLOT_FIELD_ACTION;
      else if (rule_file_v2_key_is(key, equals, slot, "delayMs")) field = RULE_FILE_V4_SLOT_FIELD_DELAY;
      else if (rule_file_v2_key_is(key, equals, slot, "timeoutMs")) field = RULE_FILE_V4_SLOT_FIELD_TIMEOUT;
      else if (rule_file_v2_key_is(key, equals, slot, "safeState")) field = RULE_FILE_V4_SLOT_FIELD_SAFE_STATE;
      else if (rule_file_v2_key_is(key, equals, slot, "priority")) field = RULE_FILE_V4_SLOT_FIELD_PRIORITY;
      if (field == 0u) {
        continue;
      }
      if ((slot_fields[slot] & field) != 0u) {
        return false;
      }
      if (field == RULE_FILE_V4_SLOT_FIELD_SIGNAL_KEY) {
        if (value_len >= sizeof(parsed.slots[slot].signal_key)) {
          return false;
        }
        memcpy(parsed.slots[slot].signal_key, value_text, value_len);
        parsed.slots[slot].signal_key[value_len] = '\0';
      } else if (field == RULE_FILE_V4_SLOT_FIELD_THRESHOLD) {
        if (!rule_file_parse_decimal((const char *)value_text, value_len,
                                     &parsed.slots[slot].threshold)) {
          return false;
        }
      } else if (field == RULE_FILE_V4_SLOT_FIELD_ACTION ||
                 field == RULE_FILE_V4_SLOT_FIELD_SAFE_STATE) {
        RelayState *state = field == RULE_FILE_V4_SLOT_FIELD_ACTION ?
                              &parsed.slots[slot].action_state :
                              &parsed.slots[slot].safe_state;
        if (!rule_file_v2_parse_state(value_text, value_len, state)) {
          return false;
        }
      } else if (!rule_file_parse_u32(value_text, value_len, &value)) {
        return false;
      }
      if (field == RULE_FILE_V4_SLOT_FIELD_ENABLED) {
        if (value > 1u) return false;
        parsed.slots[slot].enabled = value != 0u;
      } else if (field == RULE_FILE_V4_SLOT_FIELD_RELAY) {
        if (value >= RULE_RELAY_COUNT) return false;
        parsed.slots[slot].relay = (uint8_t)value;
      } else if (field == RULE_FILE_V4_SLOT_FIELD_DELAY) {
        parsed.slots[slot].delay_ms = value;
      } else if (field == RULE_FILE_V4_SLOT_FIELD_TIMEOUT) {
        parsed.slots[slot].timeout_ms = value;
      } else if (field == RULE_FILE_V4_SLOT_FIELD_PRIORITY) {
        if (value > UINT8_MAX) return false;
        parsed.slots[slot].priority = (uint8_t)value;
      }
      slot_fields[slot] |= field;
      recognized = true;
      break;
    }
    if (!recognized) {
      return false;
    }
  }
  if (global_fields != (RULE_FILE_V4_FIELD_VERSION | RULE_FILE_V4_FIELD_COUNT) || version != 4u ||
      count != RULE_FILE_V2_RULE_COUNT || parsed.slots[0].priority == parsed.slots[1].priority) {
    return false;
  }
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    if (slot_fields[slot] != RULE_FILE_V4_SLOT_FIELD_ALL ||
        parsed.slots[slot].signal_key[0] == '\0' ||
        parsed.slots[slot].delay_ms > parsed.slots[slot].timeout_ms) {
      return false;
    }
  }
  *out_rules = parsed;
  return true;
}

bool rule_file_v4_build_engine(const RuleFileV4 *rules, RuleEngine *out_engine) {
  if (rules == NULL || out_engine == NULL) {
    return false;
  }
  rule_engine_init(out_engine);
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    const RuleFileV4Slot *source = &rules->slots[slot];
    Rule rule = {0};
    if (!source->enabled) {
      continue;
    }
    (void)snprintf(rule.id, sizeof(rule.id), "rule%u", (unsigned)slot);
    (void)snprintf(rule.signal_key, sizeof(rule.signal_key), "%s", source->signal_key);
    rule.enabled = true;
    rule.op = RULE_OP_GE;
    rule.threshold = source->threshold;
    rule.relay = source->relay;
    rule.action_state = source->action_state;
    rule.delay_ms = source->delay_ms;
    rule.timeout_ms = source->timeout_ms;
    rule.safe_state = source->safe_state;
    rule.default_state = RELAY_STATE_OFF;
    rule.priority = source->priority;
    if (!rule_engine_add_rule(out_engine, &rule)) {
      return false;
    }
  }
  return true;
}

size_t rule_file_format_v4(const RuleFileV4 *rules, char *out_text, size_t out_capacity) {
  size_t used;
  int header;

  if (rules == NULL || out_text == NULL || out_capacity == 0u) {
    return 0u;
  }
  header = snprintf(out_text, out_capacity, "version=4\nruleCount=2\n");
  if (header < 0 || (size_t)header >= out_capacity) {
    return 0u;
  }
  used = (size_t)header;
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    const RuleFileV4Slot *rule = &rules->slots[slot];
    char threshold[32];
    int written;
    if (rule->signal_key[0] == '\0' ||
        rule_file_format_decimal(rule->threshold, threshold, sizeof(threshold)) == 0u) {
      return 0u;
    }
    written = snprintf(out_text + used, out_capacity - used,
                       "rule%u.enabled=%u\nrule%u.relay=%u\nrule%u.signalKey=%s\n"
                       "rule%u.threshold=%s\nrule%u.action=%s\nrule%u.delayMs=%lu\n"
                       "rule%u.timeoutMs=%lu\nrule%u.safeState=%s\nrule%u.priority=%u\n",
                       (unsigned)slot, rule->enabled ? 1u : 0u,
                       (unsigned)slot, (unsigned)rule->relay,
                       (unsigned)slot, rule->signal_key,
                       (unsigned)slot, threshold,
                       (unsigned)slot, rule->action_state == RELAY_STATE_ON ? "on" : "off",
                       (unsigned)slot, (unsigned long)rule->delay_ms,
                       (unsigned)slot, (unsigned long)rule->timeout_ms,
                       (unsigned)slot, rule->safe_state == RELAY_STATE_ON ? "on" : "off",
                       (unsigned)slot, (unsigned)rule->priority);
    if (written < 0 || (size_t)written >= out_capacity - used) {
      return 0u;
    }
    used += (size_t)written;
  }
  return used;
}
