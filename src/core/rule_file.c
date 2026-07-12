#include "rule_file.h"

#include <limits.h>
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
