#include "signal_api.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  char *data;
  size_t capacity;
  size_t length;
} JsonBuffer;

static bool append_char(JsonBuffer *buffer, char value) {
  if (buffer->length + 1u >= buffer->capacity) {
    return false;
  }
  buffer->data[buffer->length++] = value;
  buffer->data[buffer->length] = '\0';
  return true;
}

static bool append_text(JsonBuffer *buffer, const char *text) {
  const size_t len = strlen(text);
  if (len >= buffer->capacity - buffer->length) {
    return false;
  }
  memcpy(&buffer->data[buffer->length], text, len);
  buffer->length += len;
  buffer->data[buffer->length] = '\0';
  return true;
}

static bool append_unsigned(JsonBuffer *buffer, uint64_t value) {
  char digits[20];
  size_t length = 0u;
  do {
    digits[length++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  while (length > 0u) {
    if (!append_char(buffer, digits[--length])) {
      return false;
    }
  }
  return true;
}

static bool append_signed(JsonBuffer *buffer, int64_t value) {
  if (value < 0) {
    if (!append_char(buffer, '-')) {
      return false;
    }
    return append_unsigned(buffer, (uint64_t)(-(value + 1)) + 1u);
  }
  return append_unsigned(buffer, (uint64_t)value);
}

static bool append_physical(JsonBuffer *buffer, double value) {
  if (value > (double)INT64_MAX || value < (double)INT64_MIN) {
    return append_text(buffer, "null");
  }

  const bool negative = value < 0.0;
  const double magnitude = negative ? -value : value;
  const uint64_t whole = (uint64_t)magnitude;
  uint32_t fraction = (uint32_t)((magnitude - (double)whole) * 1000000.0 + 0.5);
  if (fraction == 1000000u) {
    fraction = 0u;
    if (negative && !append_char(buffer, '-')) {
      return false;
    }
    if (!append_unsigned(buffer, whole + 1u)) {
      return false;
    }
  } else {
    if (negative && !append_char(buffer, '-')) {
      return false;
    }
    if (!append_unsigned(buffer, whole)) {
      return false;
    }
  }
  if (!append_char(buffer, '.')) {
    return false;
  }
  uint32_t divisor = 100000u;
  do {
    if (!append_char(buffer, (char)('0' + fraction / divisor))) {
      return false;
    }
    fraction %= divisor;
    divisor /= 10u;
  } while (divisor != 0u);
  return true;
}

static bool append_escaped(JsonBuffer *buffer, const char *text) {
  if (!append_char(buffer, '"')) {
    return false;
  }
  while (*text != '\0') {
    if (*text == '"' || *text == '\\') {
      if (!append_char(buffer, '\\') || !append_char(buffer, *text)) {
        return false;
      }
    } else if ((unsigned char)*text < 0x20u) {
      return false;
    } else if (!append_char(buffer, *text)) {
      return false;
    }
    ++text;
  }
  return append_char(buffer, '"');
}

static const char *quality_name(SignalQuality quality) {
  switch (quality) {
    case SIGNAL_QUALITY_OK:
      return "ok";
    case SIGNAL_QUALITY_STALE:
      return "stale";
    case SIGNAL_QUALITY_ERROR:
      return "error";
    case SIGNAL_QUALITY_MISSING:
    default:
      return "missing";
  }
}

size_t signal_api_build_json(const SignalCacheEntry *entries,
                             size_t entry_count,
                             char *body,
                             size_t body_len) {
  if (body == NULL || body_len == 0u || (entries == NULL && entry_count != 0u)) {
    return 0u;
  }

  JsonBuffer buffer = {.data = body, .capacity = body_len, .length = 0u};
  body[0] = '\0';
  const size_t count = entry_count < SIGNAL_API_MAX_ITEMS ? entry_count : SIGNAL_API_MAX_ITEMS;
  if (!append_text(&buffer, "{\"ok\":true,\"data\":{\"items\":[")) {
    return 0u;
  }
  for (size_t i = 0u; i < count; ++i) {
    const SignalCacheEntry *entry = &entries[i];
    if ((i > 0u && !append_char(&buffer, ',')) ||
        !append_text(&buffer, "{\"key\":") || !append_escaped(&buffer, entry->key) ||
        !append_text(&buffer, ",\"value\":") || !append_physical(&buffer, entry->physical_value) ||
        !append_text(&buffer, ",\"raw\":") || !append_signed(&buffer, entry->raw_value) ||
        !append_text(&buffer, ",\"unit\":") || !append_escaped(&buffer, entry->unit) ||
        !append_text(&buffer, ",\"updated_ms\":") || !append_unsigned(&buffer, entry->updated_ms) ||
        !append_text(&buffer, ",\"quality\":") || !append_escaped(&buffer, quality_name(entry->quality)) ||
        !append_char(&buffer, '}')) {
      return 0u;
    }
  }
  if (!append_text(&buffer, "],\"count\":")) {
    return 0u;
  }
  if (!append_unsigned(&buffer, count) || !append_text(&buffer, "}}")) {
    return 0u;
  }
  return buffer.length;
}
