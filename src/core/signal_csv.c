#include "signal_csv.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  char *data;
  size_t capacity;
  size_t length;
} CsvBuffer;

static int append_char(CsvBuffer *buffer, char value) {
  if (buffer->length + 1u >= buffer->capacity) {
    return 0;
  }
  buffer->data[buffer->length++] = value;
  buffer->data[buffer->length] = '\0';
  return 1;
}

static int append_text(CsvBuffer *buffer, const char *text) {
  const size_t len = strlen(text);
  if (len >= buffer->capacity - buffer->length) {
    return 0;
  }
  memcpy(&buffer->data[buffer->length], text, len);
  buffer->length += len;
  buffer->data[buffer->length] = '\0';
  return 1;
}

static int append_unsigned(CsvBuffer *buffer, uint64_t value) {
  char digits[20];
  size_t length = 0u;
  do {
    digits[length++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  while (length > 0u) {
    if (!append_char(buffer, digits[--length])) {
      return 0;
    }
  }
  return 1;
}

static int append_signed(CsvBuffer *buffer, int64_t value) {
  if (value < 0) {
    return append_char(buffer, '-') && append_unsigned(buffer, (uint64_t)(-(value + 1)) + 1u);
  }
  return append_unsigned(buffer, (uint64_t)value);
}

static int append_physical(CsvBuffer *buffer, double value) {
  if (value > (double)INT64_MAX || value < (double)INT64_MIN) {
    return append_text(buffer, "null");
  }

  const int negative = value < 0.0;
  const double magnitude = negative ? -value : value;
  const uint64_t whole = (uint64_t)magnitude;
  uint32_t fraction = (uint32_t)((magnitude - (double)whole) * 1000000.0 + 0.5);
  if (fraction == 1000000u) {
    fraction = 0u;
    if (negative && !append_char(buffer, '-')) {
      return 0;
    }
    if (!append_unsigned(buffer, whole + 1u)) {
      return 0;
    }
  } else {
    if (negative && !append_char(buffer, '-')) {
      return 0;
    }
    if (!append_unsigned(buffer, whole)) {
      return 0;
    }
  }

  if (!append_char(buffer, '.')) {
    return 0;
  }
  uint32_t divisor = 100000u;
  do {
    if (!append_char(buffer, (char)('0' + fraction / divisor))) {
      return 0;
    }
    fraction %= divisor;
    divisor /= 10u;
  } while (divisor != 0u);
  return 1;
}

static int append_field(CsvBuffer *buffer, const char *text) {
  if (!append_char(buffer, '"')) {
    return 0;
  }
  while (*text != '\0') {
    if (*text == '"' && !append_char(buffer, '"')) {
      return 0;
    }
    if (!append_char(buffer, *text++)) {
      return 0;
    }
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

static int append_two_digits(CsvBuffer *buffer, uint32_t value) {
  return append_char(buffer, (char)('0' + (value / 10u) % 10u)) &&
         append_char(buffer, (char)('0' + value % 10u));
}

static int append_utc_time(CsvBuffer *buffer, uint64_t unix_ms) {
  uint64_t days = unix_ms / 86400000u;
  uint32_t time_ms = (uint32_t)(unix_ms % 86400000u);
  int year = 1970;
  uint32_t month = 1u;
  uint32_t day;
  uint32_t month_days[] = {31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u};

  while (days >= (uint64_t)((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365)) {
    days -= (uint64_t)((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365);
    ++year;
  }
  for (;;) {
    uint32_t days_in_month = month_days[month - 1u];
    if (month == 2u && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
      days_in_month = 29u;
    }
    if (days < days_in_month) {
      break;
    }
    days -= days_in_month;
    ++month;
  }
  day = (uint32_t)days + 1u;
  return append_unsigned(buffer, (uint64_t)year) && append_char(buffer, '-') &&
         append_two_digits(buffer, month) && append_char(buffer, '-') && append_two_digits(buffer, day) &&
         append_char(buffer, 'T') && append_two_digits(buffer, time_ms / 3600000u) &&
         append_char(buffer, ':') && append_two_digits(buffer, (time_ms / 60000u) % 60u) &&
         append_char(buffer, ':') && append_two_digits(buffer, (time_ms / 1000u) % 60u) &&
         append_char(buffer, '.') && append_char(buffer, (char)('0' + (time_ms / 100u) % 10u)) &&
         append_char(buffer, (char)('0' + (time_ms / 10u) % 10u)) &&
         append_char(buffer, (char)('0' + time_ms % 10u)) && append_char(buffer, 'Z');
}

size_t signal_csv_build_rows(const SignalCacheEntry *entries,
                             size_t entry_count,
                             bool include_header,
                             char *output,
                             size_t output_len) {
  if (output == NULL || output_len == 0u || (entries == NULL && entry_count != 0u)) {
    return 0u;
  }

  CsvBuffer buffer = {.data = output, .capacity = output_len, .length = 0u};
  output[0] = '\0';
  if (include_header && !append_text(&buffer, "updated_ms,key,value,raw,unit,quality\n")) {
    return 0u;
  }

  const size_t count = entry_count < SIGNAL_CSV_MAX_ITEMS ? entry_count : SIGNAL_CSV_MAX_ITEMS;
  for (size_t i = 0u; i < count; ++i) {
    const SignalCacheEntry *entry = &entries[i];
    if (!append_unsigned(&buffer, entry->updated_ms) || !append_char(&buffer, ',') ||
        !append_field(&buffer, entry->key) || !append_char(&buffer, ',') ||
        !append_physical(&buffer, entry->physical_value) || !append_char(&buffer, ',') ||
        !append_signed(&buffer, entry->raw_value) || !append_char(&buffer, ',') ||
        !append_field(&buffer, entry->unit) || !append_char(&buffer, ',') ||
        !append_field(&buffer, quality_name(entry->quality)) || !append_char(&buffer, '\n')) {
      return 0u;
    }
  }
  return buffer.length;
}

size_t signal_csv_build_rows_v2(const SignalCacheEntry *entries,
                                size_t entry_count,
                                uint64_t unix_ms,
                                bool include_header,
                                char *output,
                                size_t output_len) {
  CsvBuffer buffer;
  const size_t count = entry_count < SIGNAL_CSV_MAX_ITEMS ? entry_count : SIGNAL_CSV_MAX_ITEMS;

  if (output == NULL || output_len == 0u || (entries == NULL && entry_count != 0u)) {
    return 0u;
  }
  buffer.data = output;
  buffer.capacity = output_len;
  buffer.length = 0u;
  output[0] = '\0';
  if (include_header && !append_text(&buffer, "utc_time,unix_ms,updated_ms,key,value,raw,unit,quality\n")) {
    return 0u;
  }
  for (size_t i = 0u; i < count; ++i) {
    const SignalCacheEntry *entry = &entries[i];
    if (!append_utc_time(&buffer, unix_ms) || !append_char(&buffer, ',') ||
        !append_unsigned(&buffer, unix_ms) || !append_char(&buffer, ',') ||
        !append_unsigned(&buffer, entry->updated_ms) || !append_char(&buffer, ',') ||
        !append_field(&buffer, entry->key) || !append_char(&buffer, ',') ||
        !append_physical(&buffer, entry->physical_value) || !append_char(&buffer, ',') ||
        !append_signed(&buffer, entry->raw_value) || !append_char(&buffer, ',') ||
        !append_field(&buffer, entry->unit) || !append_char(&buffer, ',') ||
        !append_field(&buffer, quality_name(entry->quality)) || !append_char(&buffer, '\n')) {
      return 0u;
    }
  }
  return buffer.length;
}
