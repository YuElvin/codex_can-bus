#include "selected_signal_log.h"

#include <stdio.h>
#include <string.h>

#include "rule_file.h"
#include "signal_log_time.h"

typedef struct {
  char *data;
  size_t capacity;
  size_t length;
} SelectedLogBuffer;

static bool buffer_char(SelectedLogBuffer *buffer, char value) {
  if (buffer == NULL || buffer->data == NULL ||
      buffer->length + 1u >= buffer->capacity) {
    return false;
  }
  buffer->data[buffer->length++] = value;
  buffer->data[buffer->length] = '\0';
  return true;
}

static bool buffer_text(SelectedLogBuffer *buffer, const char *text) {
  if (buffer == NULL || text == NULL || buffer->length >= buffer->capacity) {
    return false;
  }
  const size_t length = strlen(text);
  if (length >= buffer->capacity - buffer->length) {
    return false;
  }
  memcpy(buffer->data + buffer->length, text, length);
  buffer->length += length;
  buffer->data[buffer->length] = '\0';
  return true;
}

static bool buffer_u64(SelectedLogBuffer *buffer, uint64_t value) {
  char digits[20];
  size_t length = 0u;
  do {
    digits[length++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  while (length > 0u) {
    if (!buffer_char(buffer, digits[--length])) {
      return false;
    }
  }
  return true;
}

static bool buffer_hex(SelectedLogBuffer *buffer, uint64_t value,
                       uint8_t digits) {
  static const char hex[] = "0123456789ABCDEF";
  for (uint8_t i = digits; i > 0u; --i) {
    if (!buffer_char(buffer,
                     hex[(value >> ((uint32_t)(i - 1u) * 4u)) & 0x0fu])) {
      return false;
    }
  }
  return true;
}

static bool buffer_fixed_decimal(SelectedLogBuffer *buffer, uint32_t value,
                                 uint8_t digits) {
  uint32_t divisor = 1u;
  for (uint8_t i = 1u; i < digits; ++i) {
    divisor *= 10u;
  }
  for (uint8_t i = 0u; i < digits; ++i) {
    if (!buffer_char(buffer, (char)('0' + (value / divisor) % 10u))) {
      return false;
    }
    divisor = divisor > 1u ? divisor / 10u : 1u;
  }
  return true;
}

static bool buffer_quoted(SelectedLogBuffer *buffer, const char *text) {
  if (!buffer_char(buffer, '"')) {
    return false;
  }
  while (text != NULL && *text != '\0') {
    if (*text == '"' && !buffer_char(buffer, '"')) {
      return false;
    }
    if (!buffer_char(buffer, *text++)) {
      return false;
    }
  }
  return buffer_char(buffer, '"');
}

static bool bounded_string(const char *text, size_t capacity,
                           bool reject_control, bool allow_empty) {
  if (text == NULL || capacity == 0u) {
    return false;
  }
  for (size_t i = 0u; i < capacity; ++i) {
    const unsigned char value = (unsigned char)text[i];
    if (value == 0u) {
      return allow_empty || i != 0u;
    }
    if (reject_control && value < 0x20u) {
      return false;
    }
  }
  return false;
}

static bool path_pair_valid(const char *csv_path, const char *meta_path) {
  if (!bounded_string(csv_path, SELECTED_SIGNAL_LOG_PATH_MAX_BYTES, true,
                      false) ||
      !bounded_string(meta_path, SELECTED_SIGNAL_LOG_PATH_MAX_BYTES, true,
                      false)) {
    return false;
  }
  const size_t csv_length = strlen(csv_path);
  const size_t meta_length = strlen(meta_path);
  return csv_length > 4u && meta_length > 5u &&
         strncmp(csv_path, "/log/", 5u) == 0 &&
         strncmp(meta_path, "/log/", 5u) == 0 &&
         strcmp(csv_path + csv_length - 4u, ".csv") == 0 &&
         strcmp(meta_path + meta_length - 5u, ".meta") == 0 &&
         csv_length - 4u == meta_length - 5u &&
         memcmp(csv_path, meta_path, csv_length - 4u) == 0;
}

static void serialization_begin(char *output, size_t output_capacity,
                                size_t *output_length) {
  if (output_length != NULL) {
    *output_length = 0u;
  }
  if (output != NULL && output_capacity != 0u) {
    output[0] = '\0';
  }
}

static SelectedSignalLogStatus serialization_finish(
  SelectedLogBuffer *buffer,
  bool success,
  size_t *output_length) {
  if (!success) {
    if (buffer != NULL && buffer->data != NULL && buffer->capacity != 0u) {
      buffer->data[0] = '\0';
      buffer->length = 0u;
    }
    return SELECTED_SIGNAL_LOG_CAPACITY;
  }
  *output_length = buffer->length;
  return SELECTED_SIGNAL_LOG_OK;
}

static bool identity_valid(const SelectedSignalLogIdentity *identity) {
  return identity != NULL &&
         identity->meta_format_version ==
           SELECTED_SIGNAL_LOG_META_FORMAT_VERSION &&
         identity->csv_format_version ==
           SELECTED_SIGNAL_LOG_CSV_FORMAT_VERSION &&
         identity->active_generation != 0u &&
         identity->candidate_generation != 0u && identity->source_size != 0u &&
         identity->source_size <= LARGE_DBC_SOURCE_MAX_BYTES &&
         selected_signal_log_admissible(identity->selected_count,
                                        identity->sample_period_ms) &&
         bounded_string(identity->firmware_build_id,
                        sizeof(identity->firmware_build_id), true, false) &&
         path_pair_valid(identity->csv_path, identity->meta_path);
}

bool selected_signal_log_admissible(uint16_t selected_count,
                                    uint32_t sample_period_ms) {
  if (selected_count == 0u ||
      selected_count > LARGE_DBC_ACTIVE_MAX_SIGNALS ||
      sample_period_ms < LARGE_DBC_LOG_SAMPLE_PERIOD_MIN_MS ||
      sample_period_ms > LARGE_DBC_LOG_SAMPLE_PERIOD_MAX_MS) {
    return false;
  }
  return (uint64_t)selected_count * UINT64_C(1000) <=
         (uint64_t)LARGE_DBC_MAX_LOG_ROWS_PER_SECOND * sample_period_ms;
}

SelectedSignalLogStatus selected_signal_log_format_paths(
  uint64_t start_unix_ms,
  int32_t utc_offset_min,
  char *csv_path,
  size_t csv_path_capacity,
  char *meta_path,
  size_t meta_path_capacity) {
  SignalLogCalendar calendar;
  if (csv_path == NULL || meta_path == NULL || csv_path_capacity == 0u ||
      meta_path_capacity == 0u ||
      !signal_log_time_local_calendar(start_unix_ms, utc_offset_min,
                                      &calendar) ||
      calendar.year > 9999u) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  const int csv_length = snprintf(
    csv_path, csv_path_capacity,
    "/log/%04u%02u%02u_%02u%02u%02u%03u_signal-v4.csv",
    (unsigned)calendar.year, (unsigned)calendar.month,
    (unsigned)calendar.day, (unsigned)calendar.hour,
    (unsigned)calendar.minute, (unsigned)calendar.second,
    (unsigned)calendar.millisecond);
  const int meta_length = snprintf(
    meta_path, meta_path_capacity,
    "/log/%04u%02u%02u_%02u%02u%02u%03u_signal-v4.meta",
    (unsigned)calendar.year, (unsigned)calendar.month,
    (unsigned)calendar.day, (unsigned)calendar.hour,
    (unsigned)calendar.minute, (unsigned)calendar.second,
    (unsigned)calendar.millisecond);
  if (csv_length < 0 || meta_length < 0 ||
      (size_t)csv_length >= csv_path_capacity ||
      (size_t)meta_length >= meta_path_capacity) {
    if (csv_path_capacity != 0u) {
      csv_path[0] = '\0';
    }
    if (meta_path_capacity != 0u) {
      meta_path[0] = '\0';
    }
    return SELECTED_SIGNAL_LOG_CAPACITY;
  }
  return SELECTED_SIGNAL_LOG_OK;
}

SelectedSignalLogStatus selected_signal_log_check_path_collision(
  bool csv_exists,
  bool meta_exists) {
  return csv_exists || meta_exists ? SELECTED_SIGNAL_LOG_PATH_COLLISION :
                                     SELECTED_SIGNAL_LOG_OK;
}

void selected_signal_log_session_init(SelectedSignalLogSession *session) {
  if (session == NULL) {
    return;
  }
  memset(session, 0, sizeof(*session));
  session->session_format_version = SELECTED_SIGNAL_LOG_META_FORMAT_VERSION;
  session->state = SELECTED_SIGNAL_LOG_STOPPED;
}

SelectedSignalLogStatus selected_signal_log_session_prepare(
  SelectedSignalLogSession *session,
  const SelectedSignalLogIdentity *identity,
  bool csv_exists,
  bool meta_exists) {
  if (session == NULL || identity == NULL) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  if (session->session_format_version !=
        SELECTED_SIGNAL_LOG_META_FORMAT_VERSION ||
      session->state != SELECTED_SIGNAL_LOG_STOPPED) {
    return SELECTED_SIGNAL_LOG_INVALID_STATE;
  }
  if (!identity_valid(identity)) {
    return selected_signal_log_admissible(identity->selected_count,
                                          identity->sample_period_ms) ?
      SELECTED_SIGNAL_LOG_INVALID_IDENTITY : SELECTED_SIGNAL_LOG_RATE_LIMIT;
  }
  const SelectedSignalLogStatus path_status =
    selected_signal_log_check_path_collision(csv_exists, meta_exists);
  if (path_status != SELECTED_SIGNAL_LOG_OK) {
    return path_status;
  }
  SelectedSignalLogSession prepared;
  selected_signal_log_session_init(&prepared);
  prepared.identity = *identity;
  prepared.state = SELECTED_SIGNAL_LOG_STARTING;
  *session = prepared;
  return SELECTED_SIGNAL_LOG_OK;
}

SelectedSignalLogStatus selected_signal_log_session_transition(
  SelectedSignalLogSession *session,
  SelectedSignalLogState next_state) {
  if (session == NULL || next_state > SELECTED_SIGNAL_LOG_FAILED) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  const SelectedSignalLogState current = (SelectedSignalLogState)session->state;
  const bool allowed =
    (current == SELECTED_SIGNAL_LOG_STARTING &&
     (next_state == SELECTED_SIGNAL_LOG_ACTIVE ||
      next_state == SELECTED_SIGNAL_LOG_FAILED)) ||
    (current == SELECTED_SIGNAL_LOG_ACTIVE &&
     (next_state == SELECTED_SIGNAL_LOG_STOPPING ||
      next_state == SELECTED_SIGNAL_LOG_FAILED)) ||
    (current == SELECTED_SIGNAL_LOG_STOPPING &&
     (next_state == SELECTED_SIGNAL_LOG_STOPPED ||
      next_state == SELECTED_SIGNAL_LOG_FAILED)) ||
    (current == SELECTED_SIGNAL_LOG_FAILED &&
     next_state == SELECTED_SIGNAL_LOG_STOPPED);
  if (!allowed) {
    return SELECTED_SIGNAL_LOG_INVALID_STATE;
  }
  session->state = (uint8_t)next_state;
  return SELECTED_SIGNAL_LOG_OK;
}

SignalValueQuality selected_signal_log_effective_quality(
  const SignalValueSnapshot *value,
  uint32_t now_ms) {
  if (value == NULL) {
    return SIGNAL_VALUE_QUALITY_ERROR;
  }
  switch (value->quality) {
    case SIGNAL_VALUE_QUALITY_GOOD:
      return (uint32_t)(now_ms - value->updated_ms) >
               SELECTED_SIGNAL_LOG_STALE_AFTER_MS ?
        SIGNAL_VALUE_QUALITY_STALE : SIGNAL_VALUE_QUALITY_GOOD;
    case SIGNAL_VALUE_QUALITY_MISSING:
    case SIGNAL_VALUE_QUALITY_STALE:
    case SIGNAL_VALUE_QUALITY_ERROR:
      return value->quality;
    default:
      return SIGNAL_VALUE_QUALITY_ERROR;
  }
}

static bool buffer_utc(SelectedLogBuffer *buffer, uint64_t unix_ms) {
  SignalLogCalendar calendar;
  if (!signal_log_time_local_calendar(unix_ms, 0, &calendar) ||
      calendar.year > 9999u) {
    return false;
  }
  return buffer_fixed_decimal(buffer, calendar.year, 4u) &&
         buffer_char(buffer, '-') &&
         buffer_fixed_decimal(buffer, calendar.month, 2u) &&
         buffer_char(buffer, '-') &&
         buffer_fixed_decimal(buffer, calendar.day, 2u) &&
         buffer_char(buffer, 'T') &&
         buffer_fixed_decimal(buffer, calendar.hour, 2u) &&
         buffer_char(buffer, ':') &&
         buffer_fixed_decimal(buffer, calendar.minute, 2u) &&
         buffer_char(buffer, ':') &&
         buffer_fixed_decimal(buffer, calendar.second, 2u) &&
         buffer_char(buffer, '.') &&
         buffer_fixed_decimal(buffer, calendar.millisecond, 3u) &&
         buffer_char(buffer, 'Z');
}

SelectedSignalLogStatus selected_signal_log_serialize_csv_header_start(
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  serialization_begin(output, output_capacity, output_length);
  if (output == NULL || output_length == NULL || output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  return serialization_finish(
    &buffer, buffer_text(&buffer, "datetime"),
    output_length);
}

SelectedSignalLogStatus selected_signal_log_serialize_csv_header_signal(
  const DbcSelectedRuntimeSignal *signal,
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  serialization_begin(output, output_capacity, output_length);
  if (signal == NULL || output == NULL ||
      output_length == NULL || output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES ||
      !bounded_string(signal->key, sizeof(signal->key), false, false)) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  return serialization_finish(
    &buffer, buffer_char(&buffer, ',') && buffer_quoted(&buffer, signal->key),
    output_length);
}

SelectedSignalLogStatus selected_signal_log_serialize_csv_row_start(
  uint64_t unix_ms,
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  serialization_begin(output, output_capacity, output_length);
  if (output == NULL || output_length == NULL || output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  return serialization_finish(&buffer, buffer_utc(&buffer, unix_ms),
                              output_length);
}

SelectedSignalLogStatus selected_signal_log_serialize_csv_row_value(
  uint32_t now_ms,
  const SignalValueSnapshot *value,
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  char physical[128];
  serialization_begin(output, output_capacity, output_length);
  if (value == NULL || output == NULL || output_length == NULL ||
      output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  const SignalValueQuality quality =
    selected_signal_log_effective_quality(value, now_ms);
  if (quality == SIGNAL_VALUE_QUALITY_GOOD ||
      quality == SIGNAL_VALUE_QUALITY_STALE) {
    if (rule_file_format_decimal(value->value, physical, sizeof(physical)) == 0u) {
      return SELECTED_SIGNAL_LOG_INVALID_VALUE;
    }
  } else {
    physical[0] = '\0';
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  const bool success =
    buffer_char(&buffer, ',') && buffer_text(&buffer, physical);
  return serialization_finish(&buffer, success, output_length);
}

SelectedSignalLogStatus selected_signal_log_serialize_csv_line_end(
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  serialization_begin(output, output_capacity, output_length);
  if (output == NULL || output_length == NULL || output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  return serialization_finish(&buffer, buffer_char(&buffer, '\n'),
                              output_length);
}

SelectedSignalLogStatus selected_signal_log_serialize_meta_header(
  const SelectedSignalLogIdentity *identity,
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  serialization_begin(output, output_capacity, output_length);
  if (!identity_valid(identity) || output == NULL || output_length == NULL ||
      output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  const bool success =
    buffer_text(&buffer, "format=selected-signal-log-meta-v1\n") &&
    buffer_text(&buffer, "csvFormat=signal-v4\nfirmwareBuildId=") &&
    buffer_quoted(&buffer, identity->firmware_build_id) &&
    buffer_text(&buffer, "\nactiveGeneration=") &&
    buffer_hex(&buffer, identity->active_generation, 16u) &&
    buffer_text(&buffer, "\ncandidateGeneration=") &&
    buffer_hex(&buffer, identity->candidate_generation, 16u) &&
    buffer_text(&buffer, "\nsourceSize=") &&
    buffer_u64(&buffer, identity->source_size) &&
    buffer_text(&buffer, "\nsourceCrc32=") &&
    buffer_hex(&buffer, identity->source_crc32, 8u) &&
    buffer_text(&buffer, "\nselectionCrc32=") &&
    buffer_hex(&buffer, identity->selection_crc32, 8u) &&
    buffer_text(&buffer, "\nselectedCount=") &&
    buffer_u64(&buffer, identity->selected_count) &&
    buffer_text(&buffer, "\nsamplePeriodMs=") &&
    buffer_u64(&buffer, identity->sample_period_ms) &&
    buffer_text(&buffer, "\nstartUnixMs=") &&
    buffer_u64(&buffer, identity->start_unix_ms) &&
    buffer_text(&buffer, "\ncleanClose=false\n");
  return serialization_finish(&buffer, success, output_length);
}

SelectedSignalLogStatus selected_signal_log_serialize_meta_signal(
  const DbcSelectedRuntimeSignal *signal,
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  serialization_begin(output, output_capacity, output_length);
  if (signal == NULL || output == NULL || output_length == NULL ||
      output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES ||
      !bounded_string(signal->key, sizeof(signal->key), false, false)) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  const bool success =
    buffer_text(&buffer, "signal=") &&
    buffer_u64(&buffer, signal->catalog_ordinal) &&
    buffer_char(&buffer, ',') &&
    buffer_hex(&buffer, signal->definition_hash, 16u) &&
    buffer_char(&buffer, ',') && buffer_quoted(&buffer, signal->key) &&
    buffer_char(&buffer, '\n');
  return serialization_finish(&buffer, success, output_length);
}

SelectedSignalLogStatus selected_signal_log_serialize_meta_footer(
  uint64_t end_unix_ms,
  bool clean_close,
  const SelectedSignalLogCounters *counters,
  char *output,
  size_t output_capacity,
  size_t *output_length) {
  serialization_begin(output, output_capacity, output_length);
  if (counters == NULL || output == NULL || output_length == NULL ||
      output_capacity == 0u ||
      output_capacity > SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES) {
    return SELECTED_SIGNAL_LOG_INVALID_ARGUMENT;
  }
  SelectedLogBuffer buffer = {output, output_capacity, 0u};
  const bool success =
    buffer_text(&buffer, "endUnixMs=") &&
    buffer_u64(&buffer, end_unix_ms) &&
    buffer_text(&buffer, "\nrowsWritten=") &&
    buffer_u64(&buffer, counters->rows_written) &&
    buffer_text(&buffer, "\nrowsDropped=") &&
    buffer_u64(&buffer, counters->rows_dropped) &&
    buffer_text(&buffer, "\nlateSamples=") &&
    buffer_u64(&buffer, counters->late_samples) &&
    buffer_text(&buffer, "\nwriteFailures=") &&
    buffer_u64(&buffer, counters->write_failures) &&
    buffer_text(&buffer, "\nflushCount=") &&
    buffer_u64(&buffer, counters->flush_count) &&
    buffer_text(&buffer, "\ncleanClose=") &&
    buffer_text(&buffer, clean_close ? "true\n" : "false\n");
  return serialization_finish(&buffer, success, output_length);
}

const char *selected_signal_log_status_string(SelectedSignalLogStatus status) {
  switch (status) {
    case SELECTED_SIGNAL_LOG_OK: return "ok";
    case SELECTED_SIGNAL_LOG_INVALID_ARGUMENT: return "invalid_argument";
    case SELECTED_SIGNAL_LOG_RATE_LIMIT: return "rate_limit";
    case SELECTED_SIGNAL_LOG_PATH_COLLISION: return "path_collision";
    case SELECTED_SIGNAL_LOG_INVALID_IDENTITY: return "invalid_identity";
    case SELECTED_SIGNAL_LOG_INVALID_STATE: return "invalid_state";
    case SELECTED_SIGNAL_LOG_CAPACITY: return "capacity";
    case SELECTED_SIGNAL_LOG_INVALID_VALUE: return "invalid_value";
    default: return "unknown";
  }
}
