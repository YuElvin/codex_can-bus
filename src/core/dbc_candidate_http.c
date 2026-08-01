#include "dbc_candidate_http.h"

#include <stdbool.h>
#include <string.h>

#define CANDIDATE_SIGNALS_PATH "/api/dbc/candidate/signals"
#define ORDINAL_LIST_TEXT_MAX_BYTES \
  (LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS * 6u - 1u)

typedef struct {
  char *output;
  size_t capacity;
  size_t length;
  bool failed;
} JsonWriter;

static bool span_equal(const char *data, size_t length, const char *literal) {
  return strlen(literal) == length && memcmp(data, literal, length) == 0;
}

static int hex_value(char value) {
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

static DbcCandidateHttpStatus decode_form_value(
  const char *encoded,
  size_t encoded_length,
  char *decoded,
  size_t decoded_capacity,
  size_t *decoded_length) {
  size_t output_length = 0u;
  for (size_t i = 0u; i < encoded_length; ++i) {
    uint8_t value = (uint8_t)encoded[i];
    if (value == (uint8_t)'%') {
      if (encoded_length - i < 3u) {
        return DBC_CANDIDATE_HTTP_INVALID_ENCODING;
      }
      const int high = hex_value(encoded[i + 1u]);
      const int low = hex_value(encoded[i + 2u]);
      if (high < 0 || low < 0) {
        return DBC_CANDIDATE_HTTP_INVALID_ENCODING;
      }
      value = (uint8_t)(((unsigned)high << 4u) | (unsigned)low);
      i += 2u;
    } else if (value == (uint8_t)'+') {
      value = (uint8_t)' ';
    }
    if (value == 0u) {
      return DBC_CANDIDATE_HTTP_INVALID_ENCODING;
    }
    if (output_length >= decoded_capacity) {
      return DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED;
    }
    decoded[output_length++] = (char)value;
  }
  *decoded_length = output_length;
  return DBC_CANDIDATE_HTTP_OK;
}

static DbcCandidateHttpStatus parse_u32(const char *data, size_t length,
                                        uint32_t *result) {
  if (length == 0u) {
    return DBC_CANDIDATE_HTTP_INVALID_NUMBER;
  }
  uint32_t value = 0u;
  for (size_t i = 0u; i < length; ++i) {
    if (data[i] < '0' || data[i] > '9') {
      return DBC_CANDIDATE_HTTP_INVALID_NUMBER;
    }
    const uint32_t digit = (uint32_t)(data[i] - '0');
    if (value > (UINT32_MAX - digit) / 10u) {
      return DBC_CANDIDATE_HTTP_INVALID_NUMBER;
    }
    value = value * 10u + digit;
  }
  *result = value;
  return DBC_CANDIDATE_HTTP_OK;
}

DbcCandidateHttpStatus dbc_candidate_http_parse_get_target(
  const char *target,
  size_t target_length,
  DbcCandidateCatalogQuery *query,
  char query_storage[DBC_CANDIDATE_HTTP_QUERY_STORAGE_BYTES]) {
  if (target == NULL || query == NULL || query_storage == NULL ||
      target_length == 0u) {
    return DBC_CANDIDATE_HTTP_INVALID_ARGUMENT;
  }

  const char *parameters = target;
  size_t parameters_length = target_length;
  if (target[0] == '/') {
    size_t path_length = target_length;
    for (size_t i = 0u; i < target_length; ++i) {
      if (target[i] == '?') {
        path_length = i;
        parameters = target + i + 1u;
        parameters_length = target_length - i - 1u;
        break;
      }
      if (target[i] == '#') {
        return DBC_CANDIDATE_HTTP_INVALID_TARGET;
      }
    }
    if (!span_equal(target, path_length, CANDIDATE_SIGNALS_PATH)) {
      return DBC_CANDIDATE_HTTP_INVALID_TARGET;
    }
    if (path_length == target_length) {
      parameters = target + target_length;
      parameters_length = 0u;
    }
  } else if (target[0] == '?') {
    parameters = target + 1u;
    parameters_length = target_length - 1u;
  }

  DbcCandidateCatalogQuery parsed = {
    .page = 0u,
    .page_size = LARGE_DBC_API_PAGE_ITEMS,
    .query = query_storage,
    .query_length = 0u,
    .selected_filter = DBC_CANDIDATE_FILTER_ALL
  };
  query_storage[0] = '\0';
  bool saw_page = false;
  bool saw_page_size = false;
  bool saw_query = false;
  bool saw_selected = false;
  size_t offset = 0u;
  while (offset < parameters_length) {
    size_t end = offset;
    while (end < parameters_length && parameters[end] != '&') {
      if (parameters[end] == '#') {
        return DBC_CANDIDATE_HTTP_INVALID_TARGET;
      }
      ++end;
    }
    if (end == offset) {
      return DBC_CANDIDATE_HTTP_INVALID_TARGET;
    }
    size_t equals = offset;
    while (equals < end && parameters[equals] != '=') {
      ++equals;
    }
    if (equals == end || equals == offset) {
      return DBC_CANDIDATE_HTTP_INVALID_TARGET;
    }
    const char *name = parameters + offset;
    const size_t name_length = equals - offset;
    const char *value = parameters + equals + 1u;
    const size_t value_length = end - equals - 1u;
    DbcCandidateHttpStatus status = DBC_CANDIDATE_HTTP_OK;
    if (span_equal(name, name_length, "page")) {
      if (saw_page) {
        return DBC_CANDIDATE_HTTP_DUPLICATE_FIELD;
      }
      saw_page = true;
      status = parse_u32(value, value_length, &parsed.page);
    } else if (span_equal(name, name_length, "pageSize")) {
      if (saw_page_size) {
        return DBC_CANDIDATE_HTTP_DUPLICATE_FIELD;
      }
      saw_page_size = true;
      uint32_t page_size = 0u;
      status = parse_u32(value, value_length, &page_size);
      if (status == DBC_CANDIDATE_HTTP_OK &&
          page_size != LARGE_DBC_API_PAGE_ITEMS) {
        status = DBC_CANDIDATE_HTTP_INVALID_NUMBER;
      }
    } else if (span_equal(name, name_length, "q")) {
      if (saw_query) {
        return DBC_CANDIDATE_HTTP_DUPLICATE_FIELD;
      }
      saw_query = true;
      status = decode_form_value(value, value_length, query_storage,
                                 LARGE_DBC_QUERY_MAX_BYTES,
                                 &parsed.query_length);
      if (status == DBC_CANDIDATE_HTTP_OK) {
        query_storage[parsed.query_length] = '\0';
      }
    } else if (span_equal(name, name_length, "selected")) {
      if (saw_selected) {
        return DBC_CANDIDATE_HTTP_DUPLICATE_FIELD;
      }
      saw_selected = true;
      if (span_equal(value, value_length, "all")) {
        parsed.selected_filter = DBC_CANDIDATE_FILTER_ALL;
      } else if (span_equal(value, value_length, "true")) {
        parsed.selected_filter = DBC_CANDIDATE_FILTER_SELECTED;
      } else if (span_equal(value, value_length, "false")) {
        parsed.selected_filter = DBC_CANDIDATE_FILTER_UNSELECTED;
      } else {
        status = DBC_CANDIDATE_HTTP_INVALID_TARGET;
      }
    } else {
      return DBC_CANDIDATE_HTTP_UNKNOWN_FIELD;
    }
    if (status != DBC_CANDIDATE_HTTP_OK) {
      return status;
    }
    offset = end + (end < parameters_length ? 1u : 0u);
  }
  *query = parsed;
  return DBC_CANDIDATE_HTTP_OK;
}

static bool ordinal_in(const uint16_t *ordinals, uint8_t count,
                       uint16_t ordinal) {
  for (uint8_t i = 0u; i < count; ++i) {
    if (ordinals[i] == ordinal) {
      return true;
    }
  }
  return false;
}

static DbcCandidateHttpStatus parse_ordinals(const char *value,
                                              size_t value_length,
                                              uint16_t *ordinals,
                                              uint8_t *count) {
  *count = 0u;
  if (value_length == 0u) {
    return DBC_CANDIDATE_HTTP_OK;
  }
  size_t offset = 0u;
  while (offset < value_length) {
    size_t end = offset;
    while (end < value_length && value[end] != ',') {
      ++end;
    }
    uint32_t parsed = 0u;
    const DbcCandidateHttpStatus status = parse_u32(
      value + offset, end - offset, &parsed);
    if (status != DBC_CANDIDATE_HTTP_OK || parsed > UINT16_MAX) {
      return DBC_CANDIDATE_HTTP_INVALID_NUMBER;
    }
    if (*count >= LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS) {
      return DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED;
    }
    const uint16_t ordinal = (uint16_t)parsed;
    if (ordinal_in(ordinals, *count, ordinal)) {
      return DBC_CANDIDATE_HTTP_MUTATION_CONFLICT;
    }
    ordinals[*count] = ordinal;
    ++*count;
    if (end == value_length) {
      break;
    }
    offset = end + 1u;
    if (offset == value_length) {
      return DBC_CANDIDATE_HTTP_INVALID_NUMBER;
    }
  }
  return DBC_CANDIDATE_HTTP_OK;
}

DbcCandidateHttpStatus dbc_candidate_http_parse_selection_form(
  const char *body,
  size_t body_length,
  DbcCandidateHttpSelectionForm *form) {
  if (body == NULL || form == NULL || body_length == 0u) {
    return DBC_CANDIDATE_HTTP_INVALID_ARGUMENT;
  }
  if (body_length > LARGE_DBC_SELECTION_REQUEST_BODY_BYTES) {
    return DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED;
  }
  /* form is the caller-owned parse workspace; its contents are undefined on
   * failure, which avoids a second full form object on the HTTP task stack. */
  memset(form, 0, sizeof(*form));
  bool saw_token = false;
  bool saw_set = false;
  bool saw_clear = false;
  char decoded[ORDINAL_LIST_TEXT_MAX_BYTES];
  size_t offset = 0u;
  while (offset < body_length) {
    size_t end = offset;
    while (end < body_length && body[end] != '&') {
      ++end;
    }
    if (end == offset) {
      return DBC_CANDIDATE_HTTP_INVALID_TARGET;
    }
    size_t equals = offset;
    while (equals < end && body[equals] != '=') {
      ++equals;
    }
    if (equals == end || equals == offset) {
      return DBC_CANDIDATE_HTTP_INVALID_TARGET;
    }
    const char *name = body + offset;
    const size_t name_length = equals - offset;
    size_t decoded_length = 0u;
    DbcCandidateHttpStatus status = decode_form_value(
      body + equals + 1u, end - equals - 1u, decoded, sizeof(decoded),
      &decoded_length);
    if (status != DBC_CANDIDATE_HTTP_OK) {
      return status;
    }
    if (span_equal(name, name_length, "candidateToken")) {
      if (saw_token) {
        return DBC_CANDIDATE_HTTP_DUPLICATE_FIELD;
      }
      saw_token = true;
      if (decoded_length == 0u ||
          decoded_length >= sizeof(form->candidate_token)) {
        return DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED;
      }
      memcpy(form->candidate_token, decoded, decoded_length);
      form->candidate_token[decoded_length] = '\0';
    } else if (span_equal(name, name_length, "set")) {
      if (saw_set) {
        return DBC_CANDIDATE_HTTP_DUPLICATE_FIELD;
      }
      saw_set = true;
      status = parse_ordinals(decoded, decoded_length, form->set_ordinals,
                              &form->set_count);
    } else if (span_equal(name, name_length, "clear")) {
      if (saw_clear) {
        return DBC_CANDIDATE_HTTP_DUPLICATE_FIELD;
      }
      saw_clear = true;
      status = parse_ordinals(decoded, decoded_length, form->clear_ordinals,
                              &form->clear_count);
    } else {
      return DBC_CANDIDATE_HTTP_UNKNOWN_FIELD;
    }
    if (status != DBC_CANDIDATE_HTTP_OK) {
      return status;
    }
    offset = end + (end < body_length ? 1u : 0u);
  }
  if (!saw_token) {
    return DBC_CANDIDATE_HTTP_INVALID_TARGET;
  }
  uint64_t token_generation = 0u;
  uint32_t token_source_size = 0u;
  uint32_t token_source_crc32 = 0u;
  if (dbc_candidate_token_parse(form->candidate_token, &token_generation,
                                &token_source_size, &token_source_crc32) !=
      DBC_CANDIDATE_FORMAT_OK) {
    return DBC_CANDIDATE_HTTP_INVALID_TOKEN;
  }
  if ((uint16_t)form->set_count + form->clear_count == 0u) {
    return DBC_CANDIDATE_HTTP_INVALID_ARGUMENT;
  }
  if ((uint16_t)form->set_count + form->clear_count >
      LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS) {
    return DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED;
  }
  for (uint8_t i = 0u; i < form->set_count; ++i) {
    if (ordinal_in(form->clear_ordinals, form->clear_count,
                   form->set_ordinals[i])) {
      return DBC_CANDIDATE_HTTP_MUTATION_CONFLICT;
    }
  }
  return DBC_CANDIDATE_HTTP_OK;
}

static void writer_bytes(JsonWriter *writer, const char *bytes, size_t length) {
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

static void writer_literal(JsonWriter *writer, const char *literal) {
  writer_bytes(writer, literal, strlen(literal));
}

static void writer_u64(JsonWriter *writer, uint64_t value) {
  char reversed[20];
  size_t count = 0u;
  do {
    reversed[count++] = (char)('0' + (char)(value % 10u));
    value /= 10u;
  } while (value != 0u);
  while (count != 0u) {
    --count;
    writer_bytes(writer, &reversed[count], 1u);
  }
}

static void writer_json_string(JsonWriter *writer, const char *value) {
  static const char hex[] = "0123456789ABCDEF";
  writer_literal(writer, "\"");
  for (size_t i = 0u; value[i] != '\0'; ++i) {
    const uint8_t byte = (uint8_t)value[i];
    if (byte == (uint8_t)'\"' || byte == (uint8_t)'\\') {
      const char escaped[2] = {'\\', (char)byte};
      writer_bytes(writer, escaped, sizeof(escaped));
    } else if (byte == (uint8_t)'\b') {
      writer_literal(writer, "\\b");
    } else if (byte == (uint8_t)'\f') {
      writer_literal(writer, "\\f");
    } else if (byte == (uint8_t)'\n') {
      writer_literal(writer, "\\n");
    } else if (byte == (uint8_t)'\r') {
      writer_literal(writer, "\\r");
    } else if (byte == (uint8_t)'\t') {
      writer_literal(writer, "\\t");
    } else if (byte < 0x20u) {
      const char escaped[6] = {
        '\\', 'u', '0', '0', hex[byte >> 4u], hex[byte & 0x0Fu]
      };
      writer_bytes(writer, escaped, sizeof(escaped));
    } else {
      writer_bytes(writer, value + i, 1u);
    }
  }
  writer_literal(writer, "\"");
}

static bool descriptor_token_matches(const char *token,
                                     const DbcCandidateDescriptor *descriptor) {
  uint64_t generation = 0u;
  uint32_t source_size = 0u;
  uint32_t source_crc32 = 0u;
  return dbc_candidate_token_parse(token, &generation, &source_size,
                                   &source_crc32) ==
           DBC_CANDIDATE_FORMAT_OK &&
         generation == descriptor->generation &&
         source_size == descriptor->source_size &&
         source_crc32 == descriptor->source_crc32;
}

static DbcCandidateHttpStatus finish_json(JsonWriter *counted,
                                          JsonWriter *written,
                                          char *output,
                                          size_t output_capacity,
                                          size_t *output_length) {
  if (counted->failed || counted->length > LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES) {
    return DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED;
  }
  if (output_capacity == 0u || counted->length >= output_capacity) {
    return DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED;
  }
  if (written->failed || written->length != counted->length) {
    return DBC_CANDIDATE_HTTP_INVALID_STATE;
  }
  output[written->length] = '\0';
  *output_length = written->length;
  return DBC_CANDIDATE_HTTP_OK;
}

static void write_catalog(JsonWriter *writer, const char *candidate_token,
                          const DbcCandidateDescriptor *descriptor,
                          const DbcCandidateCatalogPage *page) {
  writer_literal(writer, "{\"ok\":true,\"data\":{\"candidateToken\":");
  writer_json_string(writer, candidate_token);
  writer_literal(writer, ",\"page\":");
  writer_u64(writer, page->page);
  writer_literal(writer, ",\"pageSize\":");
  writer_u64(writer, page->page_size);
  writer_literal(writer, ",\"total\":");
  writer_u64(writer, page->catalog_total);
  writer_literal(writer, ",\"matched\":");
  writer_u64(writer, page->matched_total);
  writer_literal(writer, ",\"selectedCount\":");
  writer_u64(writer, descriptor->selected_count);
  writer_literal(writer, ",\"items\":[");
  for (uint8_t i = 0u; i < page->item_count; ++i) {
    if (i != 0u) {
      writer_literal(writer, ",");
    }
    writer_literal(writer, "{\"ordinal\":");
    writer_u64(writer, page->items[i].ordinal);
    writer_literal(writer, ",\"key\":");
    writer_json_string(writer, page->items[i].key);
    writer_literal(writer, ",\"selected\":");
    writer_literal(writer, page->items[i].selected ? "true" : "false");
    writer_literal(writer, "}");
  }
  writer_literal(writer, "]}}");
}

DbcCandidateHttpStatus dbc_candidate_http_serialize_catalog_json(
  const char *candidate_token,
  const DbcCandidateDescriptor *descriptor,
  const DbcCandidateCatalogPage *page,
  char *output,
  size_t output_capacity,
  size_t *written) {
  if (written != NULL) {
    *written = 0u;
  }
  if (candidate_token == NULL || descriptor == NULL || page == NULL ||
      output == NULL || written == NULL) {
    return DBC_CANDIDATE_HTTP_INVALID_ARGUMENT;
  }
  if (!descriptor_token_matches(candidate_token, descriptor) ||
      descriptor->selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      page->page_size != LARGE_DBC_API_PAGE_ITEMS ||
      page->item_count > LARGE_DBC_API_PAGE_ITEMS ||
      page->catalog_total != descriptor->catalog_signal_count ||
      page->matched_total > page->catalog_total ||
      page->item_count > page->matched_total) {
    return DBC_CANDIDATE_HTTP_INVALID_STATE;
  }
  for (uint8_t i = 0u; i < page->item_count; ++i) {
    if (page->items[i].ordinal >= page->catalog_total ||
        memchr(page->items[i].key, '\0', sizeof(page->items[i].key)) == NULL) {
      return DBC_CANDIDATE_HTTP_INVALID_STATE;
    }
  }
  JsonWriter counted = {0};
  write_catalog(&counted, candidate_token, descriptor, page);
  if (counted.failed || counted.length > LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES ||
      output_capacity == 0u || counted.length >= output_capacity) {
    if (output_capacity != 0u) {
      output[0] = '\0';
    }
    return DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED;
  }
  JsonWriter actual = {output, output_capacity - 1u, 0u, false};
  write_catalog(&actual, candidate_token, descriptor, page);
  return finish_json(&counted, &actual, output, output_capacity, written);
}

static void write_selection(JsonWriter *writer, const char *candidate_token,
                            const DbcCandidateDescriptor *descriptor) {
  writer_literal(writer, "{\"ok\":true,\"data\":{\"candidateToken\":");
  writer_json_string(writer, candidate_token);
  writer_literal(writer, ",\"generation\":");
  writer_u64(writer, descriptor->generation);
  writer_literal(writer, ",\"selectedCount\":");
  writer_u64(writer, descriptor->selected_count);
  writer_literal(writer, ",\"selectedMessageCount\":");
  writer_u64(writer, descriptor->selected_message_count);
  writer_literal(writer, "}}");
}

DbcCandidateHttpStatus dbc_candidate_http_serialize_selection_json(
  const char *candidate_token,
  const DbcCandidateDescriptor *descriptor,
  char *output,
  size_t output_capacity,
  size_t *written) {
  if (written != NULL) {
    *written = 0u;
  }
  if (candidate_token == NULL || descriptor == NULL || output == NULL ||
      written == NULL) {
    return DBC_CANDIDATE_HTTP_INVALID_ARGUMENT;
  }
  if (!descriptor_token_matches(candidate_token, descriptor) ||
      descriptor->selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      descriptor->selected_count > descriptor->catalog_signal_count ||
      descriptor->selected_message_count > LARGE_DBC_ACTIVE_MAX_MESSAGES ||
      descriptor->selected_message_count > descriptor->catalog_message_count ||
      descriptor->selected_message_count > descriptor->selected_count ||
      ((descriptor->selected_count == 0u) !=
       (descriptor->selected_message_count == 0u))) {
    return DBC_CANDIDATE_HTTP_INVALID_STATE;
  }
  JsonWriter counted = {0};
  write_selection(&counted, candidate_token, descriptor);
  if (counted.failed || counted.length > LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES ||
      output_capacity == 0u || counted.length >= output_capacity) {
    if (output_capacity != 0u) {
      output[0] = '\0';
    }
    return DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED;
  }
  JsonWriter actual = {output, output_capacity - 1u, 0u, false};
  write_selection(&actual, candidate_token, descriptor);
  return finish_json(&counted, &actual, output, output_capacity, written);
}

const char *dbc_candidate_http_status_string(DbcCandidateHttpStatus status) {
  switch (status) {
    case DBC_CANDIDATE_HTTP_OK: return "ok";
    case DBC_CANDIDATE_HTTP_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_CANDIDATE_HTTP_INVALID_TARGET: return "invalid_target";
    case DBC_CANDIDATE_HTTP_INVALID_ENCODING: return "invalid_encoding";
    case DBC_CANDIDATE_HTTP_UNKNOWN_FIELD: return "unknown_field";
    case DBC_CANDIDATE_HTTP_DUPLICATE_FIELD: return "duplicate_field";
    case DBC_CANDIDATE_HTTP_INVALID_NUMBER: return "invalid_number";
    case DBC_CANDIDATE_HTTP_INVALID_TOKEN: return "invalid_token";
    case DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED: return "limit_exceeded";
    case DBC_CANDIDATE_HTTP_MUTATION_CONFLICT: return "mutation_conflict";
    case DBC_CANDIDATE_HTTP_INVALID_STATE: return "invalid_state";
    case DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED: return "capacity_exceeded";
    default: return "unknown";
  }
}
