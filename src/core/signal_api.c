#include "signal_api.h"

#include <float.h>
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

typedef struct {
  char *output;
  size_t capacity;
  size_t length;
  bool failed;
} SignalJsonWriter;

static int hex_digit_value(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  return -1;
}

static SignalApiStatus decode_query_value(const char *input,
                                          size_t input_length,
                                          char *output,
                                          size_t output_capacity) {
  size_t written = 0u;
  for (size_t i = 0u; i < input_length; ++i) {
    uint8_t value = (uint8_t)input[i];
    if (value == (uint8_t)'+') {
      value = (uint8_t)' ';
    } else if (value == (uint8_t)'%') {
      if (i + 2u >= input_length) {
        return SIGNAL_API_INVALID_QUERY;
      }
      const int high = hex_digit_value(input[i + 1u]);
      const int low = hex_digit_value(input[i + 2u]);
      if (high < 0 || low < 0) {
        return SIGNAL_API_INVALID_QUERY;
      }
      value = (uint8_t)((high << 4) | low);
      i += 2u;
    }
    if (value == 0u || value < 0x20u || written + 1u >= output_capacity) {
      return SIGNAL_API_INVALID_QUERY;
    }
    output[written++] = (char)value;
  }
  output[written] = '\0';
  return SIGNAL_API_OK;
}

static SignalApiStatus parse_page_value(const char *input,
                                        size_t input_length,
                                        uint32_t *page) {
  if (input_length == 0u) {
    return SIGNAL_API_INVALID_PAGE;
  }
  uint32_t parsed = 0u;
  for (size_t i = 0u; i < input_length; ++i) {
    if (input[i] < '0' || input[i] > '9') {
      return SIGNAL_API_INVALID_PAGE;
    }
    const uint32_t digit = (uint32_t)(input[i] - '0');
    if (parsed > (UINT32_MAX - digit) / 10u) {
      return SIGNAL_API_INVALID_PAGE;
    }
    parsed = parsed * 10u + digit;
  }
  *page = parsed;
  return SIGNAL_API_OK;
}

SignalApiStatus signal_api_parse_get_target(const char *target,
                                             size_t target_length,
                                             SignalApiQuery *query) {
  static const char path[] = "/api/signals";
  const size_t path_length = sizeof(path) - 1u;
  if (target == NULL || query == NULL) {
    return SIGNAL_API_INVALID_ARGUMENT;
  }
  memset(query, 0, sizeof(*query));
  if (target_length < path_length ||
      memcmp(target, path, path_length) != 0) {
    return SIGNAL_API_INVALID_TARGET;
  }
  if (target_length == path_length) {
    return SIGNAL_API_OK;
  }
  if (target[path_length] != '?' || target_length == path_length + 1u) {
    return SIGNAL_API_INVALID_TARGET;
  }

  bool saw_page = false;
  bool saw_query = false;
  size_t offset = path_length + 1u;
  while (offset < target_length) {
    size_t end = offset;
    while (end < target_length && target[end] != '&') {
      ++end;
    }
    if (end == offset) {
      return SIGNAL_API_INVALID_TARGET;
    }
    size_t equals = offset;
    while (equals < end && target[equals] != '=') {
      ++equals;
    }
    if (equals == end || equals == offset) {
      return SIGNAL_API_INVALID_TARGET;
    }
    const char *value = target + equals + 1u;
    const size_t value_length = end - equals - 1u;
    SignalApiStatus status;
    if (equals - offset == 4u && memcmp(target + offset, "page", 4u) == 0) {
      if (saw_page) {
        return SIGNAL_API_DUPLICATE_FIELD;
      }
      status = parse_page_value(value, value_length, &query->page);
      saw_page = true;
    } else if (equals - offset == 1u && target[offset] == 'q') {
      if (saw_query) {
        return SIGNAL_API_DUPLICATE_FIELD;
      }
      status = decode_query_value(value, value_length, query->query,
                                  sizeof(query->query));
      saw_query = true;
    } else {
      return SIGNAL_API_UNKNOWN_FIELD;
    }
    if (status != SIGNAL_API_OK) {
      return status;
    }
    if (end == target_length) {
      break;
    }
    offset = end + 1u;
    if (offset == target_length) {
      return SIGNAL_API_INVALID_TARGET;
    }
  }
  return SIGNAL_API_OK;
}

static uint8_t fold_ascii(uint8_t value) {
  return value >= (uint8_t)'A' && value <= (uint8_t)'Z' ?
    (uint8_t)(value + ((uint8_t)'a' - (uint8_t)'A')) : value;
}

static bool contains_query(const char *text, const char *query) {
  const size_t query_length = strlen(query);
  if (query_length == 0u) {
    return true;
  }
  const size_t text_length = strlen(text);
  if (query_length > text_length) {
    return false;
  }
  for (size_t start = 0u; start <= text_length - query_length; ++start) {
    bool equal = true;
    for (size_t i = 0u; i < query_length; ++i) {
      if (fold_ascii((uint8_t)text[start + i]) !=
          fold_ascii((uint8_t)query[i])) {
        equal = false;
        break;
      }
    }
    if (equal) {
      return true;
    }
  }
  return false;
}

static SignalApiStatus effective_value(SignalValueSnapshot *value,
                                       uint32_t now_ms,
                                       uint32_t stale_after_ms) {
  if (value->quality > SIGNAL_VALUE_QUALITY_ERROR) {
    return SIGNAL_API_INVALID_STATE;
  }
  if (value->update_seq == 0u) {
    value->quality = SIGNAL_VALUE_QUALITY_MISSING;
    value->value = 0.0;
    value->raw = 0;
    value->updated_ms = 0u;
  } else if (value->quality == SIGNAL_VALUE_QUALITY_GOOD &&
             (uint32_t)(now_ms - value->updated_ms) > stale_after_ms) {
    value->quality = SIGNAL_VALUE_QUALITY_STALE;
  }
  return SIGNAL_API_OK;
}

SignalApiStatus signal_api_build_selected_page(
  const DbcSelectedRuntimeSnapshot *snapshot,
  const SignalApiQuery *query,
  uint32_t now_ms,
  uint32_t stale_after_ms,
  SignalApiPage *page) {
  if (snapshot == NULL || query == NULL || page == NULL || stale_after_ms == 0u ||
      memchr(query->query, '\0', sizeof(query->query)) == NULL) {
    return SIGNAL_API_INVALID_ARGUMENT;
  }
  memset(page, 0, sizeof(*page));
  const DbcSelectedRuntime *runtime = dbc_selected_runtime_active(snapshot);
  if (runtime == NULL || snapshot->active_slot >= 2u ||
      runtime->runtime_generation == 0u ||
      runtime->signal_count > LARGE_DBC_ACTIVE_MAX_SIGNALS) {
    return SIGNAL_API_INVALID_STATE;
  }
  const uint8_t active_slot = snapshot->active_slot;
  page->generation = runtime->runtime_generation;
  page->selection_crc32 = runtime->selection_crc32;
  page->page = query->page;
  page->page_size = LARGE_DBC_API_PAGE_ITEMS;
  page->total = runtime->signal_count;
  const uint64_t page_start =
    (uint64_t)query->page * (uint64_t)LARGE_DBC_API_PAGE_ITEMS;

  for (uint16_t i = 0u; i < runtime->signal_count; ++i) {
    const DbcSelectedRuntimeSignal *signal = &runtime->signals[i];
    if (signal->value_state_index >= runtime->signal_count ||
        memchr(signal->key, '\0', sizeof(signal->key)) == NULL ||
        memchr(signal->unit, '\0', sizeof(signal->unit)) == NULL) {
      memset(page, 0, sizeof(*page));
      return SIGNAL_API_INVALID_STATE;
    }
    if (!contains_query(signal->key, query->query)) {
      continue;
    }
    const uint16_t match_index = page->matched++;
    if ((uint64_t)match_index < page_start ||
        (uint64_t)match_index >= page_start + LARGE_DBC_API_PAGE_ITEMS) {
      continue;
    }
    SignalApiPageItem *item = &page->items[page->item_count];
    item->signal = signal;
    if (!dbc_selected_runtime_copy_active_value(snapshot,
                                                signal->value_state_index,
                                                &item->value)) {
      memset(page, 0, sizeof(*page));
      return SIGNAL_API_SNAPSHOT_BUSY;
    }
    const SignalApiStatus status =
      effective_value(&item->value, now_ms, stale_after_ms);
    if (status != SIGNAL_API_OK) {
      memset(page, 0, sizeof(*page));
      return status;
    }
    ++page->item_count;
  }
  const DbcSelectedRuntime *current = dbc_selected_runtime_active(snapshot);
  if (current != runtime || snapshot->active_slot != active_slot ||
      current->runtime_generation != page->generation) {
    memset(page, 0, sizeof(*page));
    return SIGNAL_API_SNAPSHOT_BUSY;
  }
  return SIGNAL_API_OK;
}

static void signal_writer_bytes(SignalJsonWriter *writer,
                                const char *bytes,
                                size_t length) {
  if (writer->failed || length > SIZE_MAX - writer->length) {
    writer->failed = true;
    return;
  }
  if (writer->output != NULL) {
    if (writer->length > writer->capacity ||
        length > writer->capacity - writer->length) {
      writer->failed = true;
      return;
    }
    memcpy(writer->output + writer->length, bytes, length);
  }
  writer->length += length;
}

static void signal_writer_literal(SignalJsonWriter *writer,
                                  const char *literal) {
  signal_writer_bytes(writer, literal, strlen(literal));
}

static void signal_writer_u64(SignalJsonWriter *writer, uint64_t value) {
  char reversed[20];
  size_t count = 0u;
  do {
    reversed[count++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  while (count != 0u) {
    signal_writer_bytes(writer, &reversed[--count], 1u);
  }
}

static void signal_writer_i64(SignalJsonWriter *writer, int64_t value) {
  if (value < 0) {
    signal_writer_literal(writer, "-");
    signal_writer_u64(writer, (uint64_t)(-(value + 1)) + 1u);
  } else {
    signal_writer_u64(writer, (uint64_t)value);
  }
}

static void signal_writer_hex(SignalJsonWriter *writer,
                              uint64_t value,
                              size_t digits) {
  static const char hex[] = "0123456789ABCDEF";
  signal_writer_literal(writer, "\"");
  for (size_t remaining = digits; remaining != 0u; --remaining) {
    const unsigned shift = (unsigned)((remaining - 1u) * 4u);
    const char digit = hex[(value >> shift) & 0x0fu];
    signal_writer_bytes(writer, &digit, 1u);
  }
  signal_writer_literal(writer, "\"");
}

static void signal_writer_string(SignalJsonWriter *writer, const char *value) {
  static const char hex[] = "0123456789ABCDEF";
  signal_writer_literal(writer, "\"");
  for (size_t i = 0u; value[i] != '\0'; ++i) {
    const uint8_t byte = (uint8_t)value[i];
    if (byte == (uint8_t)'\"' || byte == (uint8_t)'\\') {
      const char escaped[2] = {'\\', (char)byte};
      signal_writer_bytes(writer, escaped, sizeof(escaped));
    } else if (byte == (uint8_t)'\b') {
      signal_writer_literal(writer, "\\b");
    } else if (byte == (uint8_t)'\f') {
      signal_writer_literal(writer, "\\f");
    } else if (byte == (uint8_t)'\n') {
      signal_writer_literal(writer, "\\n");
    } else if (byte == (uint8_t)'\r') {
      signal_writer_literal(writer, "\\r");
    } else if (byte == (uint8_t)'\t') {
      signal_writer_literal(writer, "\\t");
    } else if (byte < 0x20u) {
      const char escaped[6] = {
        '\\', 'u', '0', '0', hex[byte >> 4u], hex[byte & 0x0fu]
      };
      signal_writer_bytes(writer, escaped, sizeof(escaped));
    } else {
      signal_writer_bytes(writer, value + i, 1u);
    }
  }
  signal_writer_literal(writer, "\"");
}

static bool physical_is_finite(double value) {
  return value == value && value <= DBL_MAX && value >= -DBL_MAX;
}

static void signal_writer_scientific(SignalJsonWriter *writer, double value) {
  bool negative = value < 0.0;
  double normalized = negative ? -value : value;
  int exponent = 0;
  while (normalized >= 10.0) {
    normalized /= 10.0;
    ++exponent;
  }
  while (normalized < 1.0) {
    normalized *= 10.0;
    --exponent;
  }
  uint64_t scaled = (uint64_t)(normalized * 10000000000000000.0 + 0.5);
  if (scaled >= UINT64_C(100000000000000000)) {
    scaled /= 10u;
    ++exponent;
  }
  if (negative) {
    signal_writer_literal(writer, "-");
  }
  uint64_t divisor = UINT64_C(10000000000000000);
  char digit = (char)('0' + scaled / divisor);
  signal_writer_bytes(writer, &digit, 1u);
  signal_writer_literal(writer, ".");
  scaled %= divisor;
  for (unsigned i = 0u; i < 16u; ++i) {
    divisor /= 10u;
    digit = (char)('0' + scaled / divisor);
    signal_writer_bytes(writer, &digit, 1u);
    scaled %= divisor;
  }
  signal_writer_literal(writer, "E");
  if (exponent < 0) {
    signal_writer_literal(writer, "-");
    signal_writer_u64(writer, (uint64_t)(-exponent));
  } else {
    signal_writer_literal(writer, "+");
    signal_writer_u64(writer, (uint64_t)exponent);
  }
}

static void signal_writer_physical(SignalJsonWriter *writer, double value) {
  if (!physical_is_finite(value)) {
    writer->failed = true;
    return;
  }
  if (value == 0.0) {
    signal_writer_literal(writer, "0.000000");
    return;
  }
  const bool negative = value < 0.0;
  const double magnitude = negative ? -value : value;
  if (magnitude < 0.000001 || magnitude > 9000000000000000000.0) {
    signal_writer_scientific(writer, value);
    return;
  }
  uint64_t whole = (uint64_t)magnitude;
  uint32_t fraction =
    (uint32_t)((magnitude - (double)whole) * 1000000.0 + 0.5);
  if (fraction == 1000000u) {
    fraction = 0u;
    ++whole;
  }
  if (negative) {
    signal_writer_literal(writer, "-");
  }
  signal_writer_u64(writer, whole);
  signal_writer_literal(writer, ".");
  uint32_t divisor = 100000u;
  do {
    const char digit = (char)('0' + fraction / divisor);
    signal_writer_bytes(writer, &digit, 1u);
    fraction %= divisor;
    divisor /= 10u;
  } while (divisor != 0u);
}

static const char *selected_quality_name(SignalValueQuality quality) {
  switch (quality) {
    case SIGNAL_VALUE_QUALITY_MISSING: return "MISSING";
    case SIGNAL_VALUE_QUALITY_GOOD: return "GOOD";
    case SIGNAL_VALUE_QUALITY_STALE: return "STALE";
    case SIGNAL_VALUE_QUALITY_ERROR: return "ERROR";
    default: return NULL;
  }
}

static bool page_is_valid(const SignalApiPage *page) {
  if (page == NULL || page->generation == 0u ||
      page->page_size != LARGE_DBC_API_PAGE_ITEMS ||
      page->total > LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      page->matched > page->total ||
      page->item_count > LARGE_DBC_API_PAGE_ITEMS ||
      page->item_count > page->matched) {
    return false;
  }
  for (uint8_t i = 0u; i < page->item_count; ++i) {
    const SignalApiPageItem *item = &page->items[i];
    if (item->signal == NULL ||
        memchr(item->signal->key, '\0', sizeof(item->signal->key)) == NULL ||
        memchr(item->signal->unit, '\0', sizeof(item->signal->unit)) == NULL ||
        selected_quality_name(item->value.quality) == NULL ||
        ((item->value.quality == SIGNAL_VALUE_QUALITY_GOOD ||
          item->value.quality == SIGNAL_VALUE_QUALITY_STALE) &&
         !physical_is_finite(item->value.value))) {
      return false;
    }
  }
  return true;
}

static void write_selected_page(SignalJsonWriter *writer,
                                const SignalApiPage *page) {
  signal_writer_literal(writer, "{\"ok\":true,\"data\":{\"generation\":");
  signal_writer_hex(writer, page->generation, 16u);
  signal_writer_literal(writer, ",\"selectionCrc32\":");
  signal_writer_hex(writer, page->selection_crc32, 8u);
  signal_writer_literal(writer, ",\"total\":");
  signal_writer_u64(writer, page->total);
  signal_writer_literal(writer, ",\"matched\":");
  signal_writer_u64(writer, page->matched);
  signal_writer_literal(writer, ",\"page\":");
  signal_writer_u64(writer, page->page);
  signal_writer_literal(writer, ",\"pageSize\":");
  signal_writer_u64(writer, page->page_size);
  signal_writer_literal(writer, ",\"items\":[");
  for (uint8_t i = 0u; i < page->item_count; ++i) {
    const SignalApiPageItem *item = &page->items[i];
    if (i != 0u) {
      signal_writer_literal(writer, ",");
    }
    signal_writer_literal(writer, "{\"ordinal\":");
    signal_writer_u64(writer, item->signal->catalog_ordinal);
    signal_writer_literal(writer, ",\"key\":");
    signal_writer_string(writer, item->signal->key);
    signal_writer_literal(writer, ",\"value\":");
    if (item->value.quality == SIGNAL_VALUE_QUALITY_MISSING ||
        item->value.quality == SIGNAL_VALUE_QUALITY_ERROR) {
      signal_writer_literal(writer, "null");
    } else {
      signal_writer_physical(writer, item->value.value);
    }
    signal_writer_literal(writer, ",\"raw\":");
    if (item->value.quality == SIGNAL_VALUE_QUALITY_MISSING ||
        item->value.quality == SIGNAL_VALUE_QUALITY_ERROR) {
      signal_writer_literal(writer, "null");
    } else {
      signal_writer_i64(writer, item->value.raw);
    }
    signal_writer_literal(writer, ",\"unit\":");
    signal_writer_string(writer, item->signal->unit);
    signal_writer_literal(writer, ",\"quality\":");
    signal_writer_string(writer, selected_quality_name(item->value.quality));
    signal_writer_literal(writer, ",\"updatedMs\":");
    signal_writer_u64(writer, item->value.updated_ms);
    signal_writer_literal(writer, "}");
  }
  signal_writer_literal(writer, "]}}");
}

SignalApiStatus signal_api_serialize_selected_page(const SignalApiPage *page,
                                                   char *body,
                                                   size_t body_len,
                                                   size_t *written) {
  if (written != NULL) {
    *written = 0u;
  }
  if (body != NULL && body_len != 0u) {
    body[0] = '\0';
  }
  if (page == NULL || body == NULL || written == NULL || body_len == 0u) {
    return SIGNAL_API_INVALID_ARGUMENT;
  }
  if (!page_is_valid(page)) {
    return SIGNAL_API_INVALID_STATE;
  }
  SignalJsonWriter counted = {0};
  write_selected_page(&counted, page);
  if (counted.failed ||
      counted.length > LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES ||
      counted.length >= body_len) {
    return counted.failed ? SIGNAL_API_INVALID_STATE :
      SIGNAL_API_CAPACITY_EXCEEDED;
  }
  SignalJsonWriter actual = {
    .output = body,
    .capacity = body_len - 1u,
    .length = 0u,
    .failed = false
  };
  write_selected_page(&actual, page);
  if (actual.failed || actual.length != counted.length) {
    body[0] = '\0';
    return SIGNAL_API_INVALID_STATE;
  }
  body[actual.length] = '\0';
  *written = actual.length;
  return SIGNAL_API_OK;
}

const char *signal_api_status_string(SignalApiStatus status) {
  switch (status) {
    case SIGNAL_API_OK: return "ok";
    case SIGNAL_API_INVALID_ARGUMENT: return "invalid_argument";
    case SIGNAL_API_INVALID_TARGET: return "invalid_target";
    case SIGNAL_API_UNKNOWN_FIELD: return "unknown_field";
    case SIGNAL_API_DUPLICATE_FIELD: return "duplicate_field";
    case SIGNAL_API_INVALID_PAGE: return "invalid_page";
    case SIGNAL_API_INVALID_QUERY: return "invalid_query";
    case SIGNAL_API_INVALID_STATE: return "invalid_state";
    case SIGNAL_API_SNAPSHOT_BUSY: return "snapshot_busy";
    case SIGNAL_API_CAPACITY_EXCEEDED: return "capacity_exceeded";
    default: return "unknown";
  }
}
