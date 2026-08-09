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

#define RULE_FILE_DECIMAL_BASE 1000000000u
#define RULE_FILE_DECIMAL_LIMBS 36u

typedef struct {
  uint32_t limb[RULE_FILE_DECIMAL_LIMBS];
} RuleFileDecimalInteger;

static bool rule_file_decimal_multiply_two(RuleFileDecimalInteger *number) {
  uint32_t carry = 0u;
  for (size_t i = 0u; i < RULE_FILE_DECIMAL_LIMBS; ++i) {
    const uint64_t product = (uint64_t)number->limb[i] * 2u + carry;
    number->limb[i] = (uint32_t)(product % RULE_FILE_DECIMAL_BASE);
    carry = (uint32_t)(product / RULE_FILE_DECIMAL_BASE);
  }
  return carry == 0u;
}

static bool rule_file_decimal_add_one(RuleFileDecimalInteger *number) {
  for (size_t i = 0u; i < RULE_FILE_DECIMAL_LIMBS; ++i) {
    if (++number->limb[i] < RULE_FILE_DECIMAL_BASE) {
      return true;
    }
    number->limb[i] = 0u;
  }
  return false;
}

static bool rule_file_decimal_feed_bit(RuleFileDecimalInteger *number,
                                       uint32_t bit) {
  return rule_file_decimal_multiply_two(number) &&
         (bit == 0u || rule_file_decimal_add_one(number));
}

static uint32_t rule_file_word_bit(const uint32_t words[3], uint32_t bit) {
  return bit < 96u ? (words[bit / 32u] >> (bit % 32u)) & 1u : 0u;
}

static bool rule_file_lower_bits_nonzero(const uint32_t words[3],
                                         uint32_t bit_count) {
  const uint32_t full_words = bit_count / 32u;
  const uint32_t partial_bits = bit_count % 32u;
  for (uint32_t i = 0u; i < full_words && i < 3u; ++i) {
    if (words[i] != 0u) {
      return true;
    }
  }
  return full_words < 3u && partial_bits != 0u &&
         (words[full_words] & ((1u << partial_bits) - 1u)) != 0u;
}

static bool rule_file_decimal_scaled(double value,
                                     RuleFileDecimalInteger *scaled,
                                     bool *negative) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof(bits));
  *negative = (bits >> 63u) != 0u;
  bits &= UINT64_C(0x7fffffffffffffff);
  const uint32_t raw_exponent = (uint32_t)(bits >> 52u);
  const uint64_t fraction = bits & UINT64_C(0x000fffffffffffff);
  if (raw_exponent == 0x7ffu) {
    return false;
  }
  memset(scaled, 0, sizeof(*scaled));
  if (raw_exponent == 0u && fraction == 0u) {
    return true;
  }

  const uint64_t mantissa = raw_exponent == 0u ? fraction :
    fraction | UINT64_C(0x0010000000000000);
  const int32_t exponent = raw_exponent == 0u ? -1074 :
    (int32_t)raw_exponent - 1023 - 52;
  if (exponent >= 0) {
    for (int32_t bit = 52; bit >= 0; --bit) {
      if (!rule_file_decimal_feed_bit(
            scaled, (uint32_t)((mantissa >> (uint32_t)bit) & 1u))) {
        return false;
      }
    }
    for (int32_t shift = 0; shift < exponent; ++shift) {
      if (!rule_file_decimal_multiply_two(scaled)) {
        return false;
      }
    }
    if (scaled->limb[RULE_FILE_DECIMAL_LIMBS - 1u] != 0u) {
      return false;
    }
    for (size_t i = RULE_FILE_DECIMAL_LIMBS - 1u; i > 0u; --i) {
      scaled->limb[i] = scaled->limb[i - 1u];
    }
    scaled->limb[0] = 0u;
    return true;
  }

  const uint64_t low_product =
    (mantissa & UINT64_C(0xffffffff)) * RULE_FILE_DECIMAL_BASE;
  const uint64_t high_product =
    (mantissa >> 32u) * RULE_FILE_DECIMAL_BASE;
  const uint64_t middle = (low_product >> 32u) +
                          (uint32_t)high_product;
  const uint32_t numerator[3] = {
    (uint32_t)low_product,
    (uint32_t)middle,
    (uint32_t)((high_product >> 32u) + (middle >> 32u))
  };
  const uint32_t shift = (uint32_t)(-exponent);
  if (shift < 96u) {
    for (int32_t bit = 95; bit >= (int32_t)shift; --bit) {
      if (!rule_file_decimal_feed_bit(
            scaled, rule_file_word_bit(numerator, (uint32_t)bit))) {
        return false;
      }
    }
  }
  const bool half_bit = shift > 0u && shift <= 96u &&
                        rule_file_word_bit(numerator, shift - 1u) != 0u;
  const bool lower_nonzero = shift > 1u &&
    rule_file_lower_bits_nonzero(numerator, shift - 1u);
  const bool quotient_odd = shift < 96u &&
                            rule_file_word_bit(numerator, shift) != 0u;
  return !half_bit || (!lower_nonzero && !quotient_odd) ||
         rule_file_decimal_add_one(scaled);
}

static size_t rule_file_u32_digits(uint32_t value) {
  size_t digits = 1u;
  while (value >= 10u) {
    value /= 10u;
    ++digits;
  }
  return digits;
}

static size_t rule_file_write_u32(char *output, uint32_t value,
                                  size_t width) {
  size_t digits = rule_file_u32_digits(value);
  const size_t written = width == 0u ? digits : width;
  for (size_t i = 0u; i < written; ++i) {
    output[written - i - 1u] = (char)('0' + (value % 10u));
    value /= 10u;
  }
  return written;
}

size_t rule_file_format_decimal(double value, char *out_text, size_t out_capacity) {
  RuleFileDecimalInteger scaled;
  bool negative;
  if (out_text == NULL || out_capacity == 0u ||
      !rule_file_decimal_scaled(value, &scaled, &negative)) {
    return 0u;
  }

  size_t highest_integer_limb = RULE_FILE_DECIMAL_LIMBS;
  while (highest_integer_limb > 1u &&
         scaled.limb[highest_integer_limb - 1u] == 0u) {
    --highest_integer_limb;
  }
  const size_t integer_digits = highest_integer_limb == 1u ? 1u :
    rule_file_u32_digits(scaled.limb[highest_integer_limb - 1u]) +
      9u * (highest_integer_limb - 2u);
  const size_t fixed_length = (negative ? 1u : 0u) + integer_digits + 10u;
  if (fixed_length >= out_capacity) {
    return 0u;
  }

  size_t used = 0u;
  if (negative) {
    out_text[used++] = '-';
  }
  if (highest_integer_limb == 1u) {
    out_text[used++] = '0';
  } else {
    used += rule_file_write_u32(out_text + used,
                                scaled.limb[highest_integer_limb - 1u], 0u);
    for (size_t i = highest_integer_limb - 1u; i > 1u; --i) {
      used += rule_file_write_u32(out_text + used, scaled.limb[i - 1u], 9u);
    }
  }
  if (scaled.limb[0] != 0u) {
    out_text[used++] = '.';
    used += rule_file_write_u32(out_text + used, scaled.limb[0], 9u);
    while (out_text[used - 1u] == '0') {
      --used;
    }
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

enum {
  RULE_FILE_V5_FIELD_VERSION = 1u << 0,
  RULE_FILE_V5_FIELD_COUNT = 1u << 1,
  RULE_FILE_V5_SLOT_FIELD_ENABLED = 1u << 0,
  RULE_FILE_V5_SLOT_FIELD_RELAY = 1u << 1,
  RULE_FILE_V5_SLOT_FIELD_SIGNAL_KEY = 1u << 2,
  RULE_FILE_V5_SLOT_FIELD_THRESHOLD = 1u << 3,
  RULE_FILE_V5_SLOT_FIELD_ACTION = 1u << 4,
  RULE_FILE_V5_SLOT_FIELD_DELAY = 1u << 5,
  RULE_FILE_V5_SLOT_FIELD_TIMEOUT = 1u << 6,
  RULE_FILE_V5_SLOT_FIELD_SAFE_STATE = 1u << 7,
  RULE_FILE_V5_SLOT_FIELD_PRIORITY = 1u << 8,
  RULE_FILE_V5_SLOT_FIELD_DEFINITION_HASH = 1u << 9,
  RULE_FILE_V5_SLOT_FIELD_ALL = RULE_FILE_V5_SLOT_FIELD_ENABLED |
                                RULE_FILE_V5_SLOT_FIELD_RELAY |
                                RULE_FILE_V5_SLOT_FIELD_SIGNAL_KEY |
                                RULE_FILE_V5_SLOT_FIELD_THRESHOLD |
                                RULE_FILE_V5_SLOT_FIELD_ACTION |
                                RULE_FILE_V5_SLOT_FIELD_DELAY |
                                RULE_FILE_V5_SLOT_FIELD_TIMEOUT |
                                RULE_FILE_V5_SLOT_FIELD_SAFE_STATE |
                                RULE_FILE_V5_SLOT_FIELD_PRIORITY |
                                RULE_FILE_V5_SLOT_FIELD_DEFINITION_HASH,
};

static bool rule_file_parse_hash64_upper(const uint8_t *text,
                                         size_t len,
                                         uint64_t *value) {
  uint64_t parsed = 0u;
  if (text == NULL || value == NULL || len != 16u) {
    return false;
  }
  for (size_t i = 0u; i < len; ++i) {
    uint8_t digit;
    if (text[i] >= (uint8_t)'0' && text[i] <= (uint8_t)'9') {
      digit = (uint8_t)(text[i] - (uint8_t)'0');
    } else if (text[i] >= (uint8_t)'A' && text[i] <= (uint8_t)'F') {
      digit = (uint8_t)(text[i] - (uint8_t)'A' + 10u);
    } else {
      return false;
    }
    parsed = (parsed << 4u) | digit;
  }
  *value = parsed;
  return true;
}

static void rule_file_format_hash64_upper(uint64_t value, char text[17]) {
  static const char digits[] = "0123456789ABCDEF";
  for (size_t i = 0u; i < 16u; ++i) {
    text[15u - i] = digits[value & UINT64_C(0x0f)];
    value >>= 4u;
  }
  text[16] = '\0';
}

static bool rule_file_signal_key_is_round_trip_safe(const char *key,
                                                    size_t capacity) {
  const char *end;
  if (key == NULL || capacity == 0u) {
    return false;
  }
  end = (const char *)memchr(key, '\0', capacity);
  if (end == NULL || end == key) {
    return false;
  }
  for (const char *cursor = key; cursor < end; ++cursor) {
    if (*cursor == '\r' || *cursor == '\n' || *cursor == '=') {
      return false;
    }
  }
  return true;
}

static bool rule_file_v4_slots_are_valid(const RuleFileV4 *rules) {
  if (rules == NULL ||
      rules->slots[0].priority == rules->slots[1].priority) {
    return false;
  }
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    const RuleFileV4Slot *rule = &rules->slots[slot];
    char threshold[32];
    if (rule->relay >= RULE_RELAY_COUNT ||
        !rule_file_signal_key_is_round_trip_safe(rule->signal_key,
                                                 sizeof(rule->signal_key)) ||
        rule_file_format_decimal(rule->threshold, threshold,
                                 sizeof(threshold)) == 0u ||
        (rule->action_state != RELAY_STATE_OFF &&
         rule->action_state != RELAY_STATE_ON) ||
        rule->delay_ms > rule->timeout_ms ||
        (rule->safe_state != RELAY_STATE_OFF &&
         rule->safe_state != RELAY_STATE_ON)) {
      return false;
    }
  }
  return true;
}

static bool rule_file_v5_slots_are_valid(const RuleFileV5 *rules) {
  if (rules == NULL ||
      rules->slots[0].priority == rules->slots[1].priority) {
    return false;
  }
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    const RuleFileV5Slot *rule = &rules->slots[slot];
    char threshold[32];
    if (rule->relay >= RULE_RELAY_COUNT ||
        !rule_file_signal_key_is_round_trip_safe(rule->signal_key,
                                                 sizeof(rule->signal_key)) ||
        rule_file_format_decimal(rule->threshold, threshold,
                                 sizeof(threshold)) == 0u ||
        (rule->action_state != RELAY_STATE_OFF &&
         rule->action_state != RELAY_STATE_ON) ||
        rule->delay_ms > rule->timeout_ms ||
        (rule->safe_state != RELAY_STATE_OFF &&
         rule->safe_state != RELAY_STATE_ON)) {
      return false;
    }
  }
  return true;
}

bool rule_file_parse_v5(const uint8_t *data, size_t len, RuleFileV5 *out_rules) {
  RuleFileV5 parsed = {0};
  uint32_t global_fields = 0u;
  uint32_t slot_fields[RULE_FILE_V2_RULE_COUNT] = {0u};
  uint32_t version = 0u;
  uint32_t count = 0u;
  size_t offset = 0u;

  if (data == NULL || out_rules == NULL || len == 0u ||
      len > RULE_FILE_V5_MAX_BYTES) {
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
    if (line_len > 0u &&
        data[line_start + line_len - 1u] == (uint8_t)'\r') {
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
      if ((global_fields & RULE_FILE_V5_FIELD_VERSION) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &version)) {
        return false;
      }
      global_fields |= RULE_FILE_V5_FIELD_VERSION;
      continue;
    }
    if (rule_file_key_is(key, equals, "ruleCount")) {
      if ((global_fields & RULE_FILE_V5_FIELD_COUNT) != 0u ||
          !rule_file_parse_u32(value_text, value_len, &count)) {
        return false;
      }
      global_fields |= RULE_FILE_V5_FIELD_COUNT;
      continue;
    }

    bool recognized = false;
    for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
      uint32_t field = 0u;
      if (rule_file_v2_key_is(key, equals, slot, "enabled")) {
        field = RULE_FILE_V5_SLOT_FIELD_ENABLED;
      } else if (rule_file_v2_key_is(key, equals, slot, "relay")) {
        field = RULE_FILE_V5_SLOT_FIELD_RELAY;
      } else if (rule_file_v2_key_is(key, equals, slot, "signalKey")) {
        field = RULE_FILE_V5_SLOT_FIELD_SIGNAL_KEY;
      } else if (rule_file_v2_key_is(key, equals, slot, "threshold")) {
        field = RULE_FILE_V5_SLOT_FIELD_THRESHOLD;
      } else if (rule_file_v2_key_is(key, equals, slot, "action")) {
        field = RULE_FILE_V5_SLOT_FIELD_ACTION;
      } else if (rule_file_v2_key_is(key, equals, slot, "delayMs")) {
        field = RULE_FILE_V5_SLOT_FIELD_DELAY;
      } else if (rule_file_v2_key_is(key, equals, slot, "timeoutMs")) {
        field = RULE_FILE_V5_SLOT_FIELD_TIMEOUT;
      } else if (rule_file_v2_key_is(key, equals, slot, "safeState")) {
        field = RULE_FILE_V5_SLOT_FIELD_SAFE_STATE;
      } else if (rule_file_v2_key_is(key, equals, slot, "priority")) {
        field = RULE_FILE_V5_SLOT_FIELD_PRIORITY;
      } else if (rule_file_v2_key_is(key, equals, slot,
                                     "definitionHash")) {
        field = RULE_FILE_V5_SLOT_FIELD_DEFINITION_HASH;
      }
      if (field == 0u) {
        continue;
      }
      if ((slot_fields[slot] & field) != 0u) {
        return false;
      }

      if (field == RULE_FILE_V5_SLOT_FIELD_SIGNAL_KEY) {
        if (value_len >= sizeof(parsed.slots[slot].signal_key)) {
          return false;
        }
        memcpy(parsed.slots[slot].signal_key, value_text, value_len);
        parsed.slots[slot].signal_key[value_len] = '\0';
      } else if (field == RULE_FILE_V5_SLOT_FIELD_THRESHOLD) {
        if (!rule_file_parse_decimal((const char *)value_text, value_len,
                                     &parsed.slots[slot].threshold)) {
          return false;
        }
      } else if (field == RULE_FILE_V5_SLOT_FIELD_ACTION ||
                 field == RULE_FILE_V5_SLOT_FIELD_SAFE_STATE) {
        RelayState *state = field == RULE_FILE_V5_SLOT_FIELD_ACTION ?
          &parsed.slots[slot].action_state : &parsed.slots[slot].safe_state;
        if (!rule_file_v2_parse_state(value_text, value_len, state)) {
          return false;
        }
      } else if (field == RULE_FILE_V5_SLOT_FIELD_DEFINITION_HASH) {
        if (!rule_file_parse_hash64_upper(
              value_text, value_len, &parsed.slots[slot].definition_hash)) {
          return false;
        }
      } else if (!rule_file_parse_u32(value_text, value_len, &value)) {
        return false;
      }

      if (field == RULE_FILE_V5_SLOT_FIELD_ENABLED) {
        if (value > 1u) {
          return false;
        }
        parsed.slots[slot].enabled = value != 0u;
      } else if (field == RULE_FILE_V5_SLOT_FIELD_RELAY) {
        if (value >= RULE_RELAY_COUNT) {
          return false;
        }
        parsed.slots[slot].relay = (uint8_t)value;
      } else if (field == RULE_FILE_V5_SLOT_FIELD_DELAY) {
        parsed.slots[slot].delay_ms = value;
      } else if (field == RULE_FILE_V5_SLOT_FIELD_TIMEOUT) {
        parsed.slots[slot].timeout_ms = value;
      } else if (field == RULE_FILE_V5_SLOT_FIELD_PRIORITY) {
        if (value > UINT8_MAX) {
          return false;
        }
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

  if (global_fields !=
        (RULE_FILE_V5_FIELD_VERSION | RULE_FILE_V5_FIELD_COUNT) ||
      version != 5u || count != RULE_FILE_V2_RULE_COUNT) {
    return false;
  }
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    if (slot_fields[slot] != RULE_FILE_V5_SLOT_FIELD_ALL) {
      return false;
    }
  }
  if (!rule_file_v5_slots_are_valid(&parsed)) {
    return false;
  }
  *out_rules = parsed;
  return true;
}

size_t rule_file_format_v5(const RuleFileV5 *rules,
                           char *out_text,
                           size_t out_capacity) {
  size_t used;
  int header;

  if (rules == NULL || out_text == NULL || out_capacity == 0u ||
      !rule_file_v5_slots_are_valid(rules)) {
    return 0u;
  }
  header = snprintf(out_text, out_capacity, "version=5\nruleCount=2\n");
  if (header < 0 || (size_t)header >= out_capacity) {
    return 0u;
  }
  used = (size_t)header;
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    const RuleFileV5Slot *rule = &rules->slots[slot];
    char threshold[32];
    char definition_hash[17];
    int written;
    if (rule_file_format_decimal(rule->threshold, threshold,
                                 sizeof(threshold)) == 0u) {
      return 0u;
    }
    rule_file_format_hash64_upper(rule->definition_hash, definition_hash);
    written = snprintf(
      out_text + used, out_capacity - used,
      "rule%u.enabled=%u\nrule%u.relay=%u\nrule%u.signalKey=%s\n"
      "rule%u.threshold=%s\nrule%u.action=%s\nrule%u.delayMs=%lu\n"
      "rule%u.timeoutMs=%lu\nrule%u.safeState=%s\nrule%u.priority=%u\n"
      "rule%u.definitionHash=%s\n",
      (unsigned)slot, rule->enabled ? 1u : 0u,
      (unsigned)slot, (unsigned)rule->relay,
      (unsigned)slot, rule->signal_key,
      (unsigned)slot, threshold,
      (unsigned)slot,
      rule->action_state == RELAY_STATE_ON ? "on" : "off",
      (unsigned)slot, (unsigned long)rule->delay_ms,
      (unsigned)slot, (unsigned long)rule->timeout_ms,
      (unsigned)slot,
      rule->safe_state == RELAY_STATE_ON ? "on" : "off",
      (unsigned)slot, (unsigned)rule->priority,
      (unsigned)slot, definition_hash);
    if (written < 0 || (size_t)written >= out_capacity - used) {
      return 0u;
    }
    used += (size_t)written;
  }
  return used <= RULE_FILE_V5_MAX_BYTES ? used : 0u;
}

bool rule_file_v5_build_engine(const RuleFileV5 *rules,
                               RuleEngine *out_engine) {
  if (rules == NULL || out_engine == NULL ||
      !rule_file_v5_slots_are_valid(rules)) {
    return false;
  }
  RuleFileV4 legacy = {0};
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    legacy.slots[slot].enabled = rules->slots[slot].enabled;
    legacy.slots[slot].relay = rules->slots[slot].relay;
    memcpy(legacy.slots[slot].signal_key, rules->slots[slot].signal_key,
           sizeof(legacy.slots[slot].signal_key));
    legacy.slots[slot].threshold = rules->slots[slot].threshold;
    legacy.slots[slot].action_state = rules->slots[slot].action_state;
    legacy.slots[slot].delay_ms = rules->slots[slot].delay_ms;
    legacy.slots[slot].timeout_ms = rules->slots[slot].timeout_ms;
    legacy.slots[slot].safe_state = rules->slots[slot].safe_state;
    legacy.slots[slot].priority = rules->slots[slot].priority;
  }
  return rule_file_v4_build_engine(&legacy, out_engine);
}

RuleFileV5MigrationStatus rule_file_v5_migrate_disabled_v4(
  const RuleFileV4 *v4,
  RuleFileV5 *out_v5) {
  RuleFileV5 migrated = {0};

  if (v4 == NULL || out_v5 == NULL) {
    return RULE_FILE_V5_MIGRATION_INVALID_ARGUMENT;
  }
  if (!rule_file_v4_slots_are_valid(v4)) {
    return RULE_FILE_V5_MIGRATION_INVALID_V4;
  }
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    if (v4->slots[slot].enabled) {
      return RULE_FILE_V5_MIGRATION_ENABLED_RULE_REQUIRES_DEFINITION_HASH;
    }
  }
  for (size_t slot = 0u; slot < RULE_FILE_V2_RULE_COUNT; ++slot) {
    const RuleFileV4Slot *source = &v4->slots[slot];
    RuleFileV5Slot *destination = &migrated.slots[slot];
    destination->enabled = false;
    destination->relay = source->relay;
    memcpy(destination->signal_key, source->signal_key,
           sizeof(destination->signal_key));
    destination->threshold = source->threshold;
    destination->action_state = source->action_state;
    destination->delay_ms = source->delay_ms;
    destination->timeout_ms = source->timeout_ms;
    destination->safe_state = source->safe_state;
    destination->priority = source->priority;
    destination->definition_hash = 0u;
  }
  *out_v5 = migrated;
  return RULE_FILE_V5_MIGRATION_OK;
}
