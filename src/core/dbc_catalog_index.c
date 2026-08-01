#include "dbc_catalog_index.h"

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(double) == 8u, "index v1 requires 64-bit double");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "index v1 requires IEEE-754 binary64");
_Static_assert(sizeof(DbcCatalogIndexVerifyWorkspace) == 2048u,
               "duplicate workspace budget changed");
_Static_assert(sizeof(DbcCatalogIndexBuilder) < 4096u,
               "builder must stay below 4 KiB");

#define BLOOM_BITS (DBC_CATALOG_INDEX_BLOOM_BYTES * 8u)
#define BLOOM_MASK (BLOOM_BITS - 1u)

static uint16_t get_u16(const uint8_t *data) {
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8u));
}

static uint32_t get_u32(const uint8_t *data) {
  return (uint32_t)data[0] |
         ((uint32_t)data[1] << 8u) |
         ((uint32_t)data[2] << 16u) |
         ((uint32_t)data[3] << 24u);
}

static uint64_t get_u64(const uint8_t *data) {
  return (uint64_t)get_u32(data) | ((uint64_t)get_u32(data + 4u) << 32u);
}

static void put_u16(uint8_t *data, uint16_t value) {
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *data, uint32_t value) {
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8u);
  data[2] = (uint8_t)(value >> 16u);
  data[3] = (uint8_t)(value >> 24u);
}

static void put_u64(uint8_t *data, uint64_t value) {
  put_u32(data, (uint32_t)value);
  put_u32(data + 4u, (uint32_t)(value >> 32u));
}

static void put_double(uint8_t *data, double value) {
  uint64_t bits = 0u;
  memcpy(&bits, &value, sizeof(bits));
  put_u64(data, bits);
}

static double get_double(const uint8_t *data) {
  const uint64_t bits = get_u64(data);
  double value = 0.0;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static uint64_t fnv1a64(const uint8_t *data, size_t size) {
  uint64_t hash = LARGE_DBC_FNV1A64_OFFSET_BASIS;
  while (size-- > 0u) {
    hash ^= *data++;
    hash *= LARGE_DBC_FNV1A64_PRIME;
  }
  return hash;
}

static double canonical_double(double value) {
  return value == 0.0 ? 0.0 : value;
}

uint64_t dbc_catalog_definition_hash_v1(
  const DbcCatalogIndexMessageInput *message,
  const DbcCatalogIndexSignalInput *signal) {
  if (message == NULL || signal == NULL) {
    return 0u;
  }
  uint8_t bytes[LARGE_DBC_DEFINITION_SERIALIZED_BYTES];
  memset(bytes, 0, sizeof(bytes));
  bytes[0] = LARGE_DBC_DEFINITION_FORMAT_VERSION;
  put_u32(bytes + 1u, message->normalized_id);
  bytes[5] = (uint8_t)((message->ide ? LARGE_DBC_SIGNAL_FLAG_IDE : 0u) |
                       (message->declared_payload_length > 8u ?
                         LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED : 0u) |
                       (signal->motorola ? LARGE_DBC_SIGNAL_FLAG_MOTOROLA : 0u) |
                       (signal->is_signed ? LARGE_DBC_SIGNAL_FLAG_SIGNED : 0u));
  bytes[6] = message->declared_payload_length;
  put_u16(bytes + 7u, signal->start_bit);
  bytes[9] = signal->bit_length;
  put_double(bytes + 10u, canonical_double(signal->factor));
  put_double(bytes + 18u, canonical_double(signal->offset));
  put_double(bytes + 26u, canonical_double(signal->minimum));
  put_double(bytes + 34u, canonical_double(signal->maximum));
  return fnv1a64(bytes, sizeof(bytes));
}

uint32_t dbc_catalog_crc32_begin(void) {
  return LARGE_DBC_CRC32_INITIAL_VALUE;
}

uint32_t dbc_catalog_crc32_update(uint32_t state,
                                  const uint8_t *data,
                                  size_t size) {
  if (data == NULL && size != 0u) {
    return state;
  }
  while (size-- > 0u) {
    state ^= *data++;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
      state = (state >> 1u) ^ ((state & 1u) != 0u ?
        LARGE_DBC_CRC32_REFLECTED_POLYNOMIAL : 0u);
    }
  }
  return state;
}

uint32_t dbc_catalog_crc32_finish(uint32_t state) {
  return state ^ LARGE_DBC_CRC32_FINAL_XOR;
}

static bool io_can_read(const DbcCatalogIndexIo *io) {
  return io != NULL && io->read_at != NULL && io->get_size != NULL;
}

static bool io_can_write(const DbcCatalogIndexIo *io) {
  return io_can_read(io) && io->write_at != NULL && io->resize != NULL;
}

static uint32_t hash_bytes(const uint8_t *data, size_t size) {
  uint32_t hash = UINT32_C(2166136261);
  while (size-- > 0u) {
    hash ^= *data++;
    hash *= UINT32_C(16777619);
  }
  return hash;
}

static uint32_t mix32(uint32_t value) {
  value ^= value >> 16u;
  value *= UINT32_C(0x7FEB352D);
  value ^= value >> 15u;
  value *= UINT32_C(0x846CA68B);
  return value ^ (value >> 16u);
}

static bool bloom_maybe_contains(const uint8_t *bits, uint32_t hash) {
  const uint32_t h1 = hash & BLOOM_MASK;
  const uint32_t h2 = mix32(hash ^ UINT32_C(0x9E3779B9)) & BLOOM_MASK;
  const uint32_t h3 = mix32(hash ^ UINT32_C(0x85EBCA6B)) & BLOOM_MASK;
  return (bits[h1 >> 3u] & (uint8_t)(1u << (h1 & 7u))) != 0u &&
         (bits[h2 >> 3u] & (uint8_t)(1u << (h2 & 7u))) != 0u &&
         (bits[h3 >> 3u] & (uint8_t)(1u << (h3 & 7u))) != 0u;
}

static void bloom_add(uint8_t *bits, uint32_t hash) {
  const uint32_t hashes[3] = {
    hash & BLOOM_MASK,
    mix32(hash ^ UINT32_C(0x9E3779B9)) & BLOOM_MASK,
    mix32(hash ^ UINT32_C(0x85EBCA6B)) & BLOOM_MASK
  };
  for (size_t i = 0u; i < 3u; ++i) {
    bits[hashes[i] >> 3u] |= (uint8_t)(1u << (hashes[i] & 7u));
  }
}

static uint32_t message_identity(uint32_t normalized_id, bool ide) {
  return normalized_id | (ide ? UINT32_C(0x40000000) : 0u);
}

static bool valid_message_identity(uint32_t normalized_id, bool ide) {
  return ide ? normalized_id <= UINT32_C(0x1FFFFFFF) : normalized_id <= 0x7ffu;
}

static bool signal_fits(uint16_t start_bit,
                        uint8_t bit_length,
                        bool motorola,
                        uint8_t payload_length) {
  const uint32_t frame_bits = (uint32_t)payload_length * 8u;
  if (bit_length == 0u || bit_length > 63u || start_bit >= frame_bits) {
    return false;
  }
  if (!motorola) {
    return (uint32_t)start_bit + bit_length <= frame_bits;
  }
  int32_t bit = start_bit;
  for (uint8_t i = 0u; i < bit_length; ++i) {
    if (bit < 0 || (uint32_t)bit >= frame_bits) {
      return false;
    }
    bit = ((bit & 7) == 0) ? bit + 15 : bit - 1;
  }
  return true;
}

static bool input_string_valid(const char *text, uint8_t length, uint8_t max_length) {
  if ((text == NULL && length != 0u) || length > max_length) {
    return false;
  }
  return length == 0u || memchr(text, '\0', length) == NULL;
}

static bool identifier_start(uint8_t value) {
  return (value >= (uint8_t)'A' && value <= (uint8_t)'Z') ||
         (value >= (uint8_t)'a' && value <= (uint8_t)'z') || value == (uint8_t)'_';
}

static bool identifier_continue(uint8_t value) {
  return identifier_start(value) ||
         (value >= (uint8_t)'0' && value <= (uint8_t)'9');
}

static bool valid_signal_key(const uint8_t *text, size_t length) {
  size_t index = 0u;
  unsigned identifiers = 0u;
  while (index < length) {
    if (!identifier_start(text[index])) {
      return false;
    }
    ++index;
    while (index < length && identifier_continue(text[index])) {
      ++index;
    }
    ++identifiers;
    if (index == length) {
      break;
    }
    if (text[index++] != (uint8_t)'.') {
      return false;
    }
  }
  return identifiers == 2u;
}

static bool valid_utf8_text(const uint8_t *text, size_t length) {
  size_t index = 0u;
  while (index < length) {
    const uint8_t first = text[index++];
    uint32_t codepoint = 0u;
    size_t continuation = 0u;
    if (first < 0x80u) {
      if (first < 0x20u || first == 0x7fu) {
        return false;
      }
      continue;
    }
    if (first >= 0xc2u && first <= 0xdfu) {
      codepoint = first & 0x1fu;
      continuation = 1u;
    } else if (first >= 0xe0u && first <= 0xefu) {
      codepoint = first & 0x0fu;
      continuation = 2u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      codepoint = first & 0x07u;
      continuation = 3u;
    } else {
      return false;
    }
    if (continuation > length - index) {
      return false;
    }
    for (size_t i = 0u; i < continuation; ++i) {
      const uint8_t next = text[index++];
      if ((next & 0xc0u) != 0x80u) {
        return false;
      }
      codepoint = (codepoint << 6u) | (next & 0x3fu);
    }
    if ((continuation == 1u && codepoint < 0x80u) ||
        (continuation == 2u && codepoint < 0x800u) ||
        (continuation == 3u && codepoint < 0x10000u) ||
        (codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
        codepoint > 0x10ffffu) {
      return false;
    }
  }
  return true;
}

static void encode_message(uint8_t record[LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE],
                           const DbcCatalogIndexMessageInput *message,
                           uint16_t first_signal,
                           uint16_t signal_count) {
  memset(record, 0, LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE);
  put_u32(record + LARGE_DBC_MESSAGE_RECORD_NORMALIZED_ID_OFFSET,
          message->normalized_id);
  put_u32(record + LARGE_DBC_MESSAGE_RECORD_SOURCE_LINE_OFFSET,
          message->source_line);
  put_u16(record + LARGE_DBC_MESSAGE_RECORD_FIRST_SIGNAL_OFFSET, first_signal);
  put_u16(record + LARGE_DBC_MESSAGE_RECORD_SIGNAL_COUNT_OFFSET, signal_count);
  record[LARGE_DBC_MESSAGE_RECORD_FLAGS_OFFSET] =
    (uint8_t)((message->ide ? LARGE_DBC_SIGNAL_FLAG_IDE : 0u) |
              (message->declared_payload_length > 8u ?
                LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED : 0u));
  record[LARGE_DBC_MESSAGE_RECORD_DECLARED_LENGTH_OFFSET] =
    message->declared_payload_length;
}

static void encode_signal(uint8_t record[LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE],
                          const DbcCatalogIndexBuilder *builder,
                          const DbcCatalogIndexSignalInput *signal) {
  memset(record, 0, LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE);
  put_u16(record + LARGE_DBC_SIGNAL_RECORD_ORDINAL_OFFSET, builder->signal_count);
  put_u16(record + LARGE_DBC_SIGNAL_RECORD_MESSAGE_ORDINAL_OFFSET,
          builder->current_message_ordinal);
  put_u32(record + LARGE_DBC_SIGNAL_RECORD_NORMALIZED_ID_OFFSET,
          builder->current_message.normalized_id);
  record[LARGE_DBC_SIGNAL_RECORD_FLAGS_OFFSET] =
    (uint8_t)((builder->current_message.ide ? LARGE_DBC_SIGNAL_FLAG_IDE : 0u) |
              (builder->current_message.declared_payload_length > 8u ?
                LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED : 0u) |
              (signal->motorola ? LARGE_DBC_SIGNAL_FLAG_MOTOROLA : 0u) |
              (signal->is_signed ? LARGE_DBC_SIGNAL_FLAG_SIGNED : 0u));
  put_u16(record + LARGE_DBC_SIGNAL_RECORD_START_BIT_OFFSET, signal->start_bit);
  record[LARGE_DBC_SIGNAL_RECORD_BIT_LENGTH_OFFSET] = signal->bit_length;
  record[LARGE_DBC_SIGNAL_RECORD_DECLARED_LENGTH_OFFSET] =
    builder->current_message.declared_payload_length;
  record[LARGE_DBC_SIGNAL_RECORD_KEY_LENGTH_OFFSET] = signal->key_length;
  record[LARGE_DBC_SIGNAL_RECORD_UNIT_LENGTH_OFFSET] = signal->unit_length;
  put_double(record + LARGE_DBC_SIGNAL_RECORD_FACTOR_OFFSET, signal->factor);
  put_double(record + LARGE_DBC_SIGNAL_RECORD_VALUE_OFFSET_OFFSET, signal->offset);
  put_double(record + LARGE_DBC_SIGNAL_RECORD_MINIMUM_OFFSET, signal->minimum);
  put_double(record + LARGE_DBC_SIGNAL_RECORD_MAXIMUM_OFFSET, signal->maximum);
  put_u64(record + LARGE_DBC_SIGNAL_RECORD_DEFINITION_HASH_OFFSET,
          signal->definition_hash);
  put_u32(record + LARGE_DBC_SIGNAL_RECORD_SOURCE_LINE_OFFSET, signal->source_line);
  put_u32(record + LARGE_DBC_SIGNAL_RECORD_SOURCE_OFFSET_OFFSET,
          signal->source_offset);
  memcpy(record + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET, signal->key, signal->key_length);
  if (signal->unit_length != 0u) {
    memcpy(record + LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET,
           signal->unit,
           signal->unit_length);
  }
}

static DbcCatalogIndexStatus flush_current_message(DbcCatalogIndexBuilder *builder) {
  if (!builder->has_current_message) {
    return DBC_CATALOG_INDEX_OK;
  }
  uint8_t record[LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE];
  encode_message(record,
                 &builder->current_message,
                 (uint16_t)(builder->signal_count - builder->current_message_signal_count),
                 builder->current_message_signal_count);
  const uint32_t offset =
    (uint32_t)builder->current_message_ordinal * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  if (!builder->message_spool.write_at(builder->message_spool.context,
                                       offset,
                                       record,
                                       sizeof(record))) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  builder->has_current_message = false;
  return DBC_CATALOG_INDEX_OK;
}

static DbcCatalogIndexStatus spool_message_duplicate(
  const DbcCatalogIndexBuilder *builder,
  uint32_t identity,
  bool *duplicate) {
  uint8_t record[LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE];
  *duplicate = false;
  for (uint16_t i = 0u; i < builder->message_count; ++i) {
    if (builder->has_current_message && i == builder->current_message_ordinal) {
      if (message_identity(builder->current_message.normalized_id,
                           builder->current_message.ide) == identity) {
        *duplicate = true;
        return DBC_CATALOG_INDEX_OK;
      }
      continue;
    }
    if (!builder->message_spool.read_at(builder->message_spool.context,
                                        (uint32_t)i * sizeof(record),
                                        record,
                                        sizeof(record))) {
      return DBC_CATALOG_INDEX_IO_ERROR;
    }
    const bool ide =
      (record[LARGE_DBC_MESSAGE_RECORD_FLAGS_OFFSET] & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u;
    if (message_identity(get_u32(record + LARGE_DBC_MESSAGE_RECORD_NORMALIZED_ID_OFFSET),
                         ide) == identity) {
      *duplicate = true;
      return DBC_CATALOG_INDEX_OK;
    }
  }
  return DBC_CATALOG_INDEX_OK;
}

static DbcCatalogIndexStatus spool_key_duplicate(const DbcCatalogIndexBuilder *builder,
                                                 const char *key,
                                                 uint8_t key_length,
                                                 bool *duplicate) {
  uint8_t record[LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE];
  *duplicate = false;
  for (uint16_t i = 0u; i < builder->signal_count; ++i) {
    if (!builder->signal_spool.read_at(builder->signal_spool.context,
                                       (uint32_t)i * sizeof(record),
                                       record,
                                       sizeof(record))) {
      return DBC_CATALOG_INDEX_IO_ERROR;
    }
    if (record[LARGE_DBC_SIGNAL_RECORD_KEY_LENGTH_OFFSET] == key_length &&
        memcmp(record + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET, key, key_length) == 0) {
      *duplicate = true;
      return DBC_CATALOG_INDEX_OK;
    }
  }
  return DBC_CATALOG_INDEX_OK;
}

DbcCatalogIndexStatus dbc_catalog_index_builder_init(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexIo *message_spool,
  const DbcCatalogIndexIo *signal_spool,
  uint32_t source_size,
  uint32_t source_crc32) {
  if (builder == NULL || !io_can_write(message_spool) || !io_can_write(signal_spool) ||
      source_size > LARGE_DBC_SOURCE_MAX_BYTES) {
    return DBC_CATALOG_INDEX_INVALID_ARGUMENT;
  }
  memset(builder, 0, sizeof(*builder));
  builder->message_spool = *message_spool;
  builder->signal_spool = *signal_spool;
  builder->source_size = source_size;
  builder->source_crc32 = source_crc32;
  if (!builder->message_spool.resize(builder->message_spool.context, 0u) ||
      !builder->signal_spool.resize(builder->signal_spool.context, 0u)) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  builder->initialized = true;
  return DBC_CATALOG_INDEX_OK;
}

DbcCatalogIndexStatus dbc_catalog_index_builder_begin_message(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexMessageInput *message) {
  if (builder == NULL || message == NULL || !builder->initialized || builder->finalized) {
    return DBC_CATALOG_INDEX_INVALID_STATE;
  }
  if (builder->message_count >= LARGE_DBC_CATALOG_MAX_MESSAGES) {
    return DBC_CATALOG_INDEX_LIMIT_EXCEEDED;
  }
  if (!valid_message_identity(message->normalized_id, message->ide) ||
      message->declared_payload_length > 64u || message->source_line == 0u) {
    return DBC_CATALOG_INDEX_INVALID_RECORD;
  }
  const uint32_t identity = message_identity(message->normalized_id, message->ide);
  const uint32_t hash = mix32(identity);
  if (bloom_maybe_contains(builder->duplicate_workspace.message_bits, hash)) {
    bool duplicate = false;
    const DbcCatalogIndexStatus checked =
      spool_message_duplicate(builder, identity, &duplicate);
    if (checked != DBC_CATALOG_INDEX_OK) {
      return checked;
    }
    if (duplicate) {
      return DBC_CATALOG_INDEX_DUPLICATE_MESSAGE;
    }
  }
  const DbcCatalogIndexStatus flushed = flush_current_message(builder);
  if (flushed != DBC_CATALOG_INDEX_OK) {
    return flushed;
  }
  bloom_add(builder->duplicate_workspace.message_bits, hash);
  builder->current_message = *message;
  builder->current_message_ordinal = builder->message_count;
  builder->current_message_signal_count = 0u;
  builder->has_current_message = true;
  ++builder->message_count;
  return DBC_CATALOG_INDEX_OK;
}

DbcCatalogIndexStatus dbc_catalog_index_builder_add_signal(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexSignalInput *signal) {
  if (builder == NULL || signal == NULL || !builder->initialized || builder->finalized ||
      !builder->has_current_message) {
    return DBC_CATALOG_INDEX_INVALID_STATE;
  }
  if (builder->signal_count >= LARGE_DBC_CATALOG_MAX_SIGNALS ||
      builder->current_message_signal_count == UINT16_MAX) {
    return DBC_CATALOG_INDEX_LIMIT_EXCEEDED;
  }
  if (!input_string_valid(signal->key, signal->key_length, LARGE_DBC_KEY_MAX_BYTES) ||
      signal->key_length == 0u ||
      !input_string_valid(signal->unit, signal->unit_length, LARGE_DBC_UNIT_MAX_BYTES) ||
      !valid_signal_key((const uint8_t *)signal->key, signal->key_length) ||
      !valid_utf8_text((const uint8_t *)signal->unit, signal->unit_length) ||
      !isfinite(signal->factor) || !isfinite(signal->offset) ||
      !isfinite(signal->minimum) || !isfinite(signal->maximum) ||
      signal->minimum > signal->maximum || signal->source_line == 0u ||
      signal->definition_hash !=
        dbc_catalog_definition_hash_v1(&builder->current_message, signal) ||
      !signal_fits(signal->start_bit,
                   signal->bit_length,
                   signal->motorola,
                   builder->current_message.declared_payload_length)) {
    return DBC_CATALOG_INDEX_INVALID_RECORD;
  }
  const uint32_t key_hash = hash_bytes((const uint8_t *)signal->key, signal->key_length);
  if (bloom_maybe_contains(builder->duplicate_workspace.signal_bits, key_hash)) {
    bool duplicate = false;
    const DbcCatalogIndexStatus checked =
      spool_key_duplicate(builder, signal->key, signal->key_length, &duplicate);
    if (checked != DBC_CATALOG_INDEX_OK) {
      return checked;
    }
    if (duplicate) {
      return DBC_CATALOG_INDEX_DUPLICATE_KEY;
    }
  }
  uint8_t record[LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE];
  encode_signal(record, builder, signal);
  if (!builder->signal_spool.write_at(builder->signal_spool.context,
                                      (uint32_t)builder->signal_count * sizeof(record),
                                      record,
                                      sizeof(record))) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  bloom_add(builder->duplicate_workspace.signal_bits, key_hash);
  ++builder->signal_count;
  ++builder->current_message_signal_count;
  return DBC_CATALOG_INDEX_OK;
}

static DbcCatalogIndexStatus crc_range(const DbcCatalogIndexIo *io,
                                       uint32_t offset,
                                       uint32_t size,
                                       uint32_t *crc_out) {
  uint8_t chunk[256];
  uint32_t crc = dbc_catalog_crc32_begin();
  while (size != 0u) {
    const uint32_t count = size < sizeof(chunk) ? size : (uint32_t)sizeof(chunk);
    if (!io->read_at(io->context, offset, chunk, count)) {
      return DBC_CATALOG_INDEX_IO_ERROR;
    }
    crc = dbc_catalog_crc32_update(crc, chunk, count);
    offset += count;
    size -= count;
  }
  *crc_out = dbc_catalog_crc32_finish(crc);
  return DBC_CATALOG_INDEX_OK;
}

static DbcCatalogIndexStatus copy_spool(const DbcCatalogIndexIo *source,
                                        uint32_t size,
                                        const DbcCatalogIndexIo *output,
                                        uint32_t output_offset) {
  uint8_t chunk[LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE];
  uint32_t source_offset = 0u;
  while (source_offset < size) {
    const uint32_t remaining = size - source_offset;
    const uint32_t count = remaining < sizeof(chunk) ? remaining : (uint32_t)sizeof(chunk);
    if (!source->read_at(source->context, source_offset, chunk, count) ||
        !output->write_at(output->context, output_offset + source_offset, chunk, count)) {
      return DBC_CATALOG_INDEX_IO_ERROR;
    }
    source_offset += count;
  }
  return DBC_CATALOG_INDEX_OK;
}

DbcCatalogIndexStatus dbc_catalog_index_builder_finalize(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexIo *output,
  DbcCatalogIndexSummary *summary) {
  if (builder == NULL || !builder->initialized || builder->finalized ||
      !io_can_write(output) || builder->message_count == 0u) {
    return DBC_CATALOG_INDEX_INVALID_STATE;
  }
  DbcCatalogIndexStatus status = flush_current_message(builder);
  if (status != DBC_CATALOG_INDEX_OK) {
    return status;
  }
  const uint32_t message_size =
    (uint32_t)builder->message_count * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  const uint32_t signal_size =
    (uint32_t)builder->signal_count * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE;
  const uint32_t signal_offset = LARGE_DBC_INDEX_HEADER_SIZE + message_size;
  const uint32_t total_size = signal_offset + signal_size;
  uint32_t actual_size = 0u;
  if (!builder->message_spool.get_size(builder->message_spool.context, &actual_size) ||
      actual_size != message_size ||
      !builder->signal_spool.get_size(builder->signal_spool.context, &actual_size) ||
      actual_size != signal_size) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  uint32_t message_crc = 0u;
  uint32_t signal_crc = 0u;
  status = crc_range(&builder->message_spool, 0u, message_size, &message_crc);
  if (status == DBC_CATALOG_INDEX_OK) {
    status = crc_range(&builder->signal_spool, 0u, signal_size, &signal_crc);
  }
  if (status != DBC_CATALOG_INDEX_OK || !output->resize(output->context, total_size)) {
    return status != DBC_CATALOG_INDEX_OK ? status : DBC_CATALOG_INDEX_IO_ERROR;
  }
  uint8_t header[LARGE_DBC_INDEX_HEADER_SIZE];
  memset(header, 0, sizeof(header));
  put_u32(header + LARGE_DBC_INDEX_MAGIC_OFFSET, LARGE_DBC_INDEX_MAGIC);
  put_u16(header + LARGE_DBC_INDEX_VERSION_OFFSET, LARGE_DBC_CONTRACT_VERSION);
  put_u16(header + LARGE_DBC_INDEX_HEADER_SIZE_OFFSET, LARGE_DBC_INDEX_HEADER_SIZE);
  put_u16(header + LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE_OFFSET,
          LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE);
  put_u16(header + LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE_OFFSET,
          LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE);
  put_u32(header + LARGE_DBC_INDEX_SOURCE_SIZE_OFFSET, builder->source_size);
  put_u32(header + LARGE_DBC_INDEX_SOURCE_CRC32_OFFSET, builder->source_crc32);
  put_u32(header + LARGE_DBC_INDEX_MESSAGE_COUNT_OFFSET, builder->message_count);
  put_u32(header + LARGE_DBC_INDEX_SIGNAL_COUNT_OFFSET, builder->signal_count);
  put_u32(header + LARGE_DBC_INDEX_MESSAGE_RECORDS_OFFSET_OFFSET,
          LARGE_DBC_INDEX_HEADER_SIZE);
  put_u32(header + LARGE_DBC_INDEX_MESSAGE_RECORDS_SIZE_OFFSET, message_size);
  put_u32(header + LARGE_DBC_INDEX_MESSAGE_RECORDS_CRC32_OFFSET, message_crc);
  put_u32(header + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET, signal_offset);
  put_u32(header + LARGE_DBC_INDEX_SIGNAL_RECORDS_SIZE_OFFSET, signal_size);
  put_u32(header + LARGE_DBC_INDEX_SIGNAL_RECORDS_CRC32_OFFSET, signal_crc);
  put_u32(header + LARGE_DBC_INDEX_TOTAL_SIZE_OFFSET, total_size);
  const uint32_t header_crc = dbc_catalog_crc32_finish(
    dbc_catalog_crc32_update(dbc_catalog_crc32_begin(),
                             header,
                             LARGE_DBC_INDEX_HEADER_CRC32_COVERED_BYTES));
  put_u32(header + LARGE_DBC_INDEX_HEADER_CRC32_OFFSET, header_crc);
  if (!output->write_at(output->context, 0u, header, sizeof(header))) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  status = copy_spool(&builder->message_spool,
                      message_size,
                      output,
                      LARGE_DBC_INDEX_HEADER_SIZE);
  if (status == DBC_CATALOG_INDEX_OK) {
    status = copy_spool(&builder->signal_spool, signal_size, output, signal_offset);
  }
  if (status != DBC_CATALOG_INDEX_OK) {
    return status;
  }
  builder->finalized = true;
  if (summary != NULL) {
    summary->source_size = builder->source_size;
    summary->source_crc32 = builder->source_crc32;
    summary->message_count = builder->message_count;
    summary->signal_count = builder->signal_count;
    summary->total_size = total_size;
    summary->message_records_crc32 = message_crc;
    summary->signal_records_crc32 = signal_crc;
  }
  return DBC_CATALOG_INDEX_OK;
}

static DbcCatalogIndexStatus decode_header(const uint8_t *header,
                                           uint32_t file_size,
                                           const DbcCatalogIndexExpectedSource *expected,
                                           DbcCatalogIndexSummary *summary) {
  if (get_u32(header + LARGE_DBC_INDEX_MAGIC_OFFSET) != LARGE_DBC_INDEX_MAGIC ||
      get_u16(header + LARGE_DBC_INDEX_VERSION_OFFSET) != LARGE_DBC_CONTRACT_VERSION ||
      get_u16(header + LARGE_DBC_INDEX_HEADER_SIZE_OFFSET) != LARGE_DBC_INDEX_HEADER_SIZE ||
      get_u16(header + LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE_OFFSET) !=
        LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE ||
      get_u16(header + LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE_OFFSET) !=
        LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE ||
      get_u32(header + LARGE_DBC_INDEX_FLAGS_OFFSET) != 0u) {
    return DBC_CATALOG_INDEX_INVALID_FORMAT;
  }
  for (size_t i = 60u; i < LARGE_DBC_INDEX_HEADER_CRC32_OFFSET; ++i) {
    if (header[i] != 0u) {
      return DBC_CATALOG_INDEX_INVALID_FORMAT;
    }
  }
  const uint32_t header_crc = dbc_catalog_crc32_finish(
    dbc_catalog_crc32_update(dbc_catalog_crc32_begin(),
                             header,
                             LARGE_DBC_INDEX_HEADER_CRC32_COVERED_BYTES));
  if (header_crc != get_u32(header + LARGE_DBC_INDEX_HEADER_CRC32_OFFSET)) {
    return DBC_CATALOG_INDEX_CRC_MISMATCH;
  }
  const uint32_t message_count = get_u32(header + LARGE_DBC_INDEX_MESSAGE_COUNT_OFFSET);
  const uint32_t signal_count = get_u32(header + LARGE_DBC_INDEX_SIGNAL_COUNT_OFFSET);
  if (message_count == 0u || message_count > LARGE_DBC_CATALOG_MAX_MESSAGES ||
      signal_count > LARGE_DBC_CATALOG_MAX_SIGNALS) {
    return DBC_CATALOG_INDEX_INVALID_FORMAT;
  }
  const uint32_t message_size = message_count * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  const uint32_t signal_size = signal_count * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE;
  const uint32_t signal_offset = LARGE_DBC_INDEX_HEADER_SIZE + message_size;
  const uint32_t total_size = signal_offset + signal_size;
  if (get_u32(header + LARGE_DBC_INDEX_MESSAGE_RECORDS_OFFSET_OFFSET) !=
        LARGE_DBC_INDEX_HEADER_SIZE ||
      get_u32(header + LARGE_DBC_INDEX_MESSAGE_RECORDS_SIZE_OFFSET) != message_size ||
      get_u32(header + LARGE_DBC_INDEX_SIGNAL_RECORDS_OFFSET_OFFSET) != signal_offset ||
      get_u32(header + LARGE_DBC_INDEX_SIGNAL_RECORDS_SIZE_OFFSET) != signal_size ||
      get_u32(header + LARGE_DBC_INDEX_TOTAL_SIZE_OFFSET) != total_size ||
      file_size != total_size) {
    return DBC_CATALOG_INDEX_INVALID_FORMAT;
  }
  const uint32_t source_size = get_u32(header + LARGE_DBC_INDEX_SOURCE_SIZE_OFFSET);
  const uint32_t source_crc = get_u32(header + LARGE_DBC_INDEX_SOURCE_CRC32_OFFSET);
  if (source_size > LARGE_DBC_SOURCE_MAX_BYTES) {
    return DBC_CATALOG_INDEX_INVALID_FORMAT;
  }
  if (expected != NULL && expected->enabled &&
      (source_size != expected->source_size || source_crc != expected->source_crc32)) {
    return DBC_CATALOG_INDEX_SOURCE_MISMATCH;
  }
  summary->source_size = source_size;
  summary->source_crc32 = source_crc;
  summary->message_count = (uint16_t)message_count;
  summary->signal_count = (uint16_t)signal_count;
  summary->total_size = total_size;
  summary->message_records_crc32 =
    get_u32(header + LARGE_DBC_INDEX_MESSAGE_RECORDS_CRC32_OFFSET);
  summary->signal_records_crc32 =
    get_u32(header + LARGE_DBC_INDEX_SIGNAL_RECORDS_CRC32_OFFSET);
  return DBC_CATALOG_INDEX_OK;
}

DbcCatalogIndexStatus dbc_catalog_index_read_message(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  uint16_t ordinal,
  DbcCatalogIndexMessage *message) {
  if (!io_can_read(index) || summary == NULL || message == NULL ||
      ordinal >= summary->message_count) {
    return DBC_CATALOG_INDEX_INVALID_ARGUMENT;
  }
  uint8_t record[LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE];
  const uint32_t offset = LARGE_DBC_INDEX_HEADER_SIZE +
    (uint32_t)ordinal * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  if (!index->read_at(index->context, offset, record, sizeof(record))) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  message->ordinal = ordinal;
  message->normalized_id = get_u32(record + LARGE_DBC_MESSAGE_RECORD_NORMALIZED_ID_OFFSET);
  message->source_line = get_u32(record + LARGE_DBC_MESSAGE_RECORD_SOURCE_LINE_OFFSET);
  message->first_signal_ordinal =
    get_u16(record + LARGE_DBC_MESSAGE_RECORD_FIRST_SIGNAL_OFFSET);
  message->signal_count = get_u16(record + LARGE_DBC_MESSAGE_RECORD_SIGNAL_COUNT_OFFSET);
  message->flags = record[LARGE_DBC_MESSAGE_RECORD_FLAGS_OFFSET];
  message->declared_payload_length =
    record[LARGE_DBC_MESSAGE_RECORD_DECLARED_LENGTH_OFFSET];
  return DBC_CATALOG_INDEX_OK;
}

DbcCatalogIndexStatus dbc_catalog_index_read_signal(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  uint16_t ordinal,
  DbcCatalogIndexSignal *signal) {
  if (!io_can_read(index) || summary == NULL || signal == NULL ||
      ordinal >= summary->signal_count) {
    return DBC_CATALOG_INDEX_INVALID_ARGUMENT;
  }
  uint8_t record[LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE];
  const uint32_t signal_offset = LARGE_DBC_INDEX_HEADER_SIZE +
    (uint32_t)summary->message_count * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  if (!index->read_at(index->context,
                      signal_offset + (uint32_t)ordinal * sizeof(record),
                      record,
                      sizeof(record))) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  memset(signal, 0, sizeof(*signal));
  signal->ordinal = get_u16(record + LARGE_DBC_SIGNAL_RECORD_ORDINAL_OFFSET);
  signal->message_ordinal =
    get_u16(record + LARGE_DBC_SIGNAL_RECORD_MESSAGE_ORDINAL_OFFSET);
  signal->normalized_id = get_u32(record + LARGE_DBC_SIGNAL_RECORD_NORMALIZED_ID_OFFSET);
  signal->flags = record[LARGE_DBC_SIGNAL_RECORD_FLAGS_OFFSET];
  signal->start_bit = get_u16(record + LARGE_DBC_SIGNAL_RECORD_START_BIT_OFFSET);
  signal->bit_length = record[LARGE_DBC_SIGNAL_RECORD_BIT_LENGTH_OFFSET];
  signal->declared_payload_length =
    record[LARGE_DBC_SIGNAL_RECORD_DECLARED_LENGTH_OFFSET];
  signal->factor = get_double(record + LARGE_DBC_SIGNAL_RECORD_FACTOR_OFFSET);
  signal->offset = get_double(record + LARGE_DBC_SIGNAL_RECORD_VALUE_OFFSET_OFFSET);
  signal->minimum = get_double(record + LARGE_DBC_SIGNAL_RECORD_MINIMUM_OFFSET);
  signal->maximum = get_double(record + LARGE_DBC_SIGNAL_RECORD_MAXIMUM_OFFSET);
  signal->definition_hash =
    get_u64(record + LARGE_DBC_SIGNAL_RECORD_DEFINITION_HASH_OFFSET);
  signal->source_line = get_u32(record + LARGE_DBC_SIGNAL_RECORD_SOURCE_LINE_OFFSET);
  signal->source_offset = get_u32(record + LARGE_DBC_SIGNAL_RECORD_SOURCE_OFFSET_OFFSET);
  const uint8_t key_length = record[LARGE_DBC_SIGNAL_RECORD_KEY_LENGTH_OFFSET];
  const uint8_t unit_length = record[LARGE_DBC_SIGNAL_RECORD_UNIT_LENGTH_OFFSET];
  if (key_length <= LARGE_DBC_KEY_MAX_BYTES && unit_length <= LARGE_DBC_UNIT_MAX_BYTES) {
    memcpy(signal->key, record + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET, key_length);
    memcpy(signal->unit, record + LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET, unit_length);
  }
  return DBC_CATALOG_INDEX_OK;
}

static DbcCatalogIndexStatus verify_string_field(const uint8_t *record,
                                                 size_t length_offset,
                                                 size_t field_offset,
                                                 size_t field_bytes,
                                                 bool require_nonempty) {
  const uint8_t length = record[length_offset];
  if (length >= field_bytes || (require_nonempty && length == 0u) ||
      memchr(record + field_offset, '\0', length) != NULL) {
    return DBC_CATALOG_INDEX_INVALID_RECORD;
  }
  for (size_t i = length; i < field_bytes; ++i) {
    if (record[field_offset + i] != 0u) {
      return DBC_CATALOG_INDEX_INVALID_RECORD;
    }
  }
  return DBC_CATALOG_INDEX_OK;
}

static DbcCatalogIndexStatus verify_message_records(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  DbcCatalogIndexVerifyWorkspace *workspace) {
  uint8_t record[LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE];
  uint8_t prior[LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE];
  uint32_t expected_first = 0u;
  for (uint16_t ordinal = 0u; ordinal < summary->message_count; ++ordinal) {
    const uint32_t offset = LARGE_DBC_INDEX_HEADER_SIZE +
      (uint32_t)ordinal * sizeof(record);
    if (!index->read_at(index->context, offset, record, sizeof(record))) {
      return DBC_CATALOG_INDEX_IO_ERROR;
    }
    const uint32_t id = get_u32(record + LARGE_DBC_MESSAGE_RECORD_NORMALIZED_ID_OFFSET);
    const uint8_t flags = record[LARGE_DBC_MESSAGE_RECORD_FLAGS_OFFSET];
    const uint8_t length = record[LARGE_DBC_MESSAGE_RECORD_DECLARED_LENGTH_OFFSET];
    const bool ide = (flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u;
    const uint16_t first = get_u16(record + LARGE_DBC_MESSAGE_RECORD_FIRST_SIGNAL_OFFSET);
    const uint16_t count = get_u16(record + LARGE_DBC_MESSAGE_RECORD_SIGNAL_COUNT_OFFSET);
    if ((flags & ~(LARGE_DBC_SIGNAL_FLAG_IDE | LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED)) != 0u ||
        record[14] != 0u || record[15] != 0u ||
        !valid_message_identity(id, ide) || length > 64u ||
        ((flags & LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED) != 0u) != (length > 8u) ||
        get_u32(record + LARGE_DBC_MESSAGE_RECORD_SOURCE_LINE_OFFSET) == 0u ||
        first != expected_first || expected_first + count > summary->signal_count) {
      return DBC_CATALOG_INDEX_INVALID_RECORD;
    }
    expected_first += count;
    const uint32_t identity = message_identity(id, ide);
    const uint32_t hash = mix32(identity);
    if (bloom_maybe_contains(workspace->message_bits, hash)) {
      for (uint16_t prior_ordinal = 0u; prior_ordinal < ordinal; ++prior_ordinal) {
        if (!index->read_at(index->context,
                            LARGE_DBC_INDEX_HEADER_SIZE +
                              (uint32_t)prior_ordinal * sizeof(prior),
                            prior,
                            sizeof(prior))) {
          return DBC_CATALOG_INDEX_IO_ERROR;
        }
        const uint8_t prior_flags = prior[LARGE_DBC_MESSAGE_RECORD_FLAGS_OFFSET];
        if (message_identity(
              get_u32(prior + LARGE_DBC_MESSAGE_RECORD_NORMALIZED_ID_OFFSET),
              (prior_flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u) == identity) {
          return DBC_CATALOG_INDEX_DUPLICATE_MESSAGE;
        }
      }
    }
    bloom_add(workspace->message_bits, hash);
  }
  return expected_first == summary->signal_count ?
    DBC_CATALOG_INDEX_OK : DBC_CATALOG_INDEX_INVALID_RECORD;
}

static DbcCatalogIndexStatus verify_signal_records(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  DbcCatalogIndexVerifyWorkspace *workspace) {
  uint8_t record[LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE];
  uint8_t prior[LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE];
  const uint32_t signal_base = LARGE_DBC_INDEX_HEADER_SIZE +
    (uint32_t)summary->message_count * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  for (uint16_t ordinal = 0u; ordinal < summary->signal_count; ++ordinal) {
    if (!index->read_at(index->context,
                        signal_base + (uint32_t)ordinal * sizeof(record),
                        record,
                        sizeof(record))) {
      return DBC_CATALOG_INDEX_IO_ERROR;
    }
    const uint16_t message_ordinal =
      get_u16(record + LARGE_DBC_SIGNAL_RECORD_MESSAGE_ORDINAL_OFFSET);
    const uint8_t flags = record[LARGE_DBC_SIGNAL_RECORD_FLAGS_OFFSET];
    const uint8_t length = record[LARGE_DBC_SIGNAL_RECORD_DECLARED_LENGTH_OFFSET];
    const uint16_t start = get_u16(record + LARGE_DBC_SIGNAL_RECORD_START_BIT_OFFSET);
    const uint8_t bits = record[LARGE_DBC_SIGNAL_RECORD_BIT_LENGTH_OFFSET];
    if (get_u16(record + LARGE_DBC_SIGNAL_RECORD_ORDINAL_OFFSET) != ordinal ||
        message_ordinal >= summary->message_count || record[9] != 0u ||
        (flags & ~(LARGE_DBC_SIGNAL_FLAG_IDE | LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED |
                   LARGE_DBC_SIGNAL_FLAG_MOTOROLA | LARGE_DBC_SIGNAL_FLAG_SIGNED)) != 0u ||
        length > 64u ||
        ((flags & LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED) != 0u) != (length > 8u) ||
        !valid_message_identity(
          get_u32(record + LARGE_DBC_SIGNAL_RECORD_NORMALIZED_ID_OFFSET),
          (flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u) ||
        !signal_fits(start,
                     bits,
                     (flags & LARGE_DBC_SIGNAL_FLAG_MOTOROLA) != 0u,
                     length) ||
        !isfinite(get_double(record + LARGE_DBC_SIGNAL_RECORD_FACTOR_OFFSET)) ||
        !isfinite(get_double(record + LARGE_DBC_SIGNAL_RECORD_VALUE_OFFSET_OFFSET)) ||
        !isfinite(get_double(record + LARGE_DBC_SIGNAL_RECORD_MINIMUM_OFFSET)) ||
        !isfinite(get_double(record + LARGE_DBC_SIGNAL_RECORD_MAXIMUM_OFFSET)) ||
        get_double(record + LARGE_DBC_SIGNAL_RECORD_MINIMUM_OFFSET) >
          get_double(record + LARGE_DBC_SIGNAL_RECORD_MAXIMUM_OFFSET) ||
        get_u32(record + LARGE_DBC_SIGNAL_RECORD_SOURCE_LINE_OFFSET) == 0u) {
      return DBC_CATALOG_INDEX_INVALID_RECORD;
    }
    DbcCatalogIndexStatus status = verify_string_field(
      record,
      LARGE_DBC_SIGNAL_RECORD_KEY_LENGTH_OFFSET,
      LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET,
      LARGE_DBC_SIGNAL_RECORD_KEY_BYTES,
      true);
    if (status == DBC_CATALOG_INDEX_OK) {
      status = verify_string_field(record,
                                   LARGE_DBC_SIGNAL_RECORD_UNIT_LENGTH_OFFSET,
                                   LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET,
                                   LARGE_DBC_SIGNAL_RECORD_UNIT_BYTES,
                                   false);
    }
    if (status == DBC_CATALOG_INDEX_OK &&
        !valid_signal_key(record + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET,
                          record[LARGE_DBC_SIGNAL_RECORD_KEY_LENGTH_OFFSET])) {
      status = DBC_CATALOG_INDEX_INVALID_RECORD;
    }
    if (status == DBC_CATALOG_INDEX_OK &&
        !valid_utf8_text(record + LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET,
                         record[LARGE_DBC_SIGNAL_RECORD_UNIT_LENGTH_OFFSET])) {
      status = DBC_CATALOG_INDEX_INVALID_RECORD;
    }
    if (status != DBC_CATALOG_INDEX_OK) {
      return status;
    }
    DbcCatalogIndexMessage message;
    status = dbc_catalog_index_read_message(index, summary, message_ordinal, &message);
    if (status != DBC_CATALOG_INDEX_OK ||
        message.normalized_id !=
          get_u32(record + LARGE_DBC_SIGNAL_RECORD_NORMALIZED_ID_OFFSET) ||
        message.flags != (flags & (LARGE_DBC_SIGNAL_FLAG_IDE |
                                   LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED)) ||
        message.declared_payload_length != length ||
        ordinal < message.first_signal_ordinal ||
        ordinal >= (uint32_t)message.first_signal_ordinal + message.signal_count) {
      return status != DBC_CATALOG_INDEX_OK ? status : DBC_CATALOG_INDEX_INVALID_RECORD;
    }
    DbcCatalogIndexMessageInput message_input = {
      .normalized_id = message.normalized_id,
      .source_line = message.source_line,
      .declared_payload_length = message.declared_payload_length,
      .ide = (message.flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u
    };
    DbcCatalogIndexSignalInput signal_input = {
      .start_bit = start,
      .bit_length = bits,
      .motorola = (flags & LARGE_DBC_SIGNAL_FLAG_MOTOROLA) != 0u,
      .is_signed = (flags & LARGE_DBC_SIGNAL_FLAG_SIGNED) != 0u,
      .factor = get_double(record + LARGE_DBC_SIGNAL_RECORD_FACTOR_OFFSET),
      .offset = get_double(record + LARGE_DBC_SIGNAL_RECORD_VALUE_OFFSET_OFFSET),
      .minimum = get_double(record + LARGE_DBC_SIGNAL_RECORD_MINIMUM_OFFSET),
      .maximum = get_double(record + LARGE_DBC_SIGNAL_RECORD_MAXIMUM_OFFSET)
    };
    if (get_u64(record + LARGE_DBC_SIGNAL_RECORD_DEFINITION_HASH_OFFSET) !=
        dbc_catalog_definition_hash_v1(&message_input, &signal_input)) {
      return DBC_CATALOG_INDEX_INVALID_RECORD;
    }
    const uint8_t key_length = record[LARGE_DBC_SIGNAL_RECORD_KEY_LENGTH_OFFSET];
    const uint8_t *key = record + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET;
    const uint32_t hash = hash_bytes(key, key_length);
    if (bloom_maybe_contains(workspace->signal_bits, hash)) {
      for (uint16_t prior_ordinal = 0u; prior_ordinal < ordinal; ++prior_ordinal) {
        if (!index->read_at(index->context,
                            signal_base + (uint32_t)prior_ordinal * sizeof(prior),
                            prior,
                            sizeof(prior))) {
          return DBC_CATALOG_INDEX_IO_ERROR;
        }
        if (prior[LARGE_DBC_SIGNAL_RECORD_KEY_LENGTH_OFFSET] == key_length &&
            memcmp(prior + LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET, key, key_length) == 0) {
          return DBC_CATALOG_INDEX_DUPLICATE_KEY;
        }
      }
    }
    bloom_add(workspace->signal_bits, hash);
  }
  return DBC_CATALOG_INDEX_OK;
}

DbcCatalogIndexStatus dbc_catalog_index_verify(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexExpectedSource *expected_source,
  DbcCatalogIndexVerifyWorkspace *workspace,
  DbcCatalogIndexSummary *summary) {
  if (!io_can_read(index) || workspace == NULL || summary == NULL) {
    return DBC_CATALOG_INDEX_INVALID_ARGUMENT;
  }
  uint32_t file_size = 0u;
  uint8_t header[LARGE_DBC_INDEX_HEADER_SIZE];
  if (!index->get_size(index->context, &file_size) ||
      file_size < sizeof(header) ||
      !index->read_at(index->context, 0u, header, sizeof(header))) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  DbcCatalogIndexStatus status =
    decode_header(header, file_size, expected_source, summary);
  if (status != DBC_CATALOG_INDEX_OK) {
    return status;
  }
  uint32_t crc = 0u;
  status = crc_range(index,
                     LARGE_DBC_INDEX_HEADER_SIZE,
                     (uint32_t)summary->message_count * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE,
                     &crc);
  if (status != DBC_CATALOG_INDEX_OK || crc != summary->message_records_crc32) {
    return status != DBC_CATALOG_INDEX_OK ? status : DBC_CATALOG_INDEX_CRC_MISMATCH;
  }
  const uint32_t signal_offset = LARGE_DBC_INDEX_HEADER_SIZE +
    (uint32_t)summary->message_count * LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE;
  status = crc_range(index,
                     signal_offset,
                     (uint32_t)summary->signal_count * LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE,
                     &crc);
  if (status != DBC_CATALOG_INDEX_OK || crc != summary->signal_records_crc32) {
    return status != DBC_CATALOG_INDEX_OK ? status : DBC_CATALOG_INDEX_CRC_MISMATCH;
  }
  memset(workspace, 0, sizeof(*workspace));
  status = verify_message_records(index, summary, workspace);
  if (status == DBC_CATALOG_INDEX_OK) {
    status = verify_signal_records(index, summary, workspace);
  }
  return status;
}

static bool dump_line(DbcCatalogIndexTextWrite write_text,
                      void *context,
                      const char *format,
                      ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  return length >= 0 && (size_t)length < sizeof(line) &&
         write_text(context, line, (size_t)length);
}

static void bytes_to_hex(const char *bytes, size_t size, char *hex) {
  static const char digits[] = "0123456789ABCDEF";
  for (size_t i = 0u; i < size; ++i) {
    const uint8_t value = (uint8_t)bytes[i];
    hex[i * 2u] = digits[value >> 4u];
    hex[i * 2u + 1u] = digits[value & 0x0fu];
  }
  hex[size * 2u] = '\0';
}

static uint64_t double_bits(double value) {
  uint64_t bits = 0u;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

DbcCatalogIndexStatus dbc_catalog_index_dump(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *verified_summary,
  DbcCatalogIndexTextWrite write_text,
  void *write_context) {
  if (!io_can_read(index) || verified_summary == NULL || write_text == NULL) {
    return DBC_CATALOG_INDEX_INVALID_ARGUMENT;
  }
  if (!dump_line(write_text,
                 write_context,
                 "INDEX v1 source_size=%lu source_crc32=%08lX messages=%u signals=%u total=%lu message_crc32=%08lX signal_crc32=%08lX\n",
                 (unsigned long)verified_summary->source_size,
                 (unsigned long)verified_summary->source_crc32,
                 (unsigned)verified_summary->message_count,
                 (unsigned)verified_summary->signal_count,
                 (unsigned long)verified_summary->total_size,
                 (unsigned long)verified_summary->message_records_crc32,
                 (unsigned long)verified_summary->signal_records_crc32)) {
    return DBC_CATALOG_INDEX_IO_ERROR;
  }
  for (uint16_t i = 0u; i < verified_summary->message_count; ++i) {
    DbcCatalogIndexMessage message;
    DbcCatalogIndexStatus status =
      dbc_catalog_index_read_message(index, verified_summary, i, &message);
    if (status != DBC_CATALOG_INDEX_OK ||
        !dump_line(write_text,
                   write_context,
                   "MESSAGE ordinal=%u id=%08lX ide=%u frame=%s length=%u source_line=%lu first_signal=%u signal_count=%u\n",
                   (unsigned)i,
                   (unsigned long)message.normalized_id,
                   (message.flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u,
                   (message.flags & LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED) != 0u ?
                     "FD_REQUIRED" : "CLASSIC_OR_FD",
                   (unsigned)message.declared_payload_length,
                   (unsigned long)message.source_line,
                   (unsigned)message.first_signal_ordinal,
                   (unsigned)message.signal_count)) {
      return status != DBC_CATALOG_INDEX_OK ? status : DBC_CATALOG_INDEX_IO_ERROR;
    }
  }
  for (uint16_t i = 0u; i < verified_summary->signal_count; ++i) {
    DbcCatalogIndexSignal signal;
    char key_hex[LARGE_DBC_SIGNAL_RECORD_KEY_BYTES * 2u + 1u];
    char unit_hex[LARGE_DBC_SIGNAL_RECORD_UNIT_BYTES * 2u + 1u];
    DbcCatalogIndexStatus status =
      dbc_catalog_index_read_signal(index, verified_summary, i, &signal);
    if (status != DBC_CATALOG_INDEX_OK) {
      return status;
    }
    bytes_to_hex(signal.key, strlen(signal.key), key_hex);
    bytes_to_hex(signal.unit, strlen(signal.unit), unit_hex);
    if (!dump_line(write_text,
                   write_context,
                   "SIGNAL ordinal=%u message=%u id=%08lX ide=%u frame=%s endian=%s signed=%u start=%u bits=%u length=%u factor_bits=%016llX offset_bits=%016llX min_bits=%016llX max_bits=%016llX definition_hash=%016llX source_line=%lu source_offset=%lu\n",
                   (unsigned)signal.ordinal,
                   (unsigned)signal.message_ordinal,
                   (unsigned long)signal.normalized_id,
                   (signal.flags & LARGE_DBC_SIGNAL_FLAG_IDE) != 0u,
                   (signal.flags & LARGE_DBC_SIGNAL_FLAG_FD_REQUIRED) != 0u ?
                     "FD_REQUIRED" : "CLASSIC_OR_FD",
                   (signal.flags & LARGE_DBC_SIGNAL_FLAG_MOTOROLA) != 0u ?
                     "MOTOROLA" : "INTEL",
                   (signal.flags & LARGE_DBC_SIGNAL_FLAG_SIGNED) != 0u,
                   (unsigned)signal.start_bit,
                   (unsigned)signal.bit_length,
                   (unsigned)signal.declared_payload_length,
                   (unsigned long long)double_bits(signal.factor),
                   (unsigned long long)double_bits(signal.offset),
                   (unsigned long long)double_bits(signal.minimum),
                   (unsigned long long)double_bits(signal.maximum),
                   (unsigned long long)signal.definition_hash,
                   (unsigned long)signal.source_line,
                   (unsigned long)signal.source_offset) ||
        !dump_line(write_text,
                   write_context,
                   "SIGNAL_TEXT ordinal=%u key_hex=%s unit_hex=%s\n",
                   (unsigned)signal.ordinal,
                   key_hex,
                   unit_hex)) {
      return DBC_CATALOG_INDEX_IO_ERROR;
    }
  }
  return DBC_CATALOG_INDEX_OK;
}

const char *dbc_catalog_index_status_string(DbcCatalogIndexStatus status) {
  switch (status) {
    case DBC_CATALOG_INDEX_OK: return "ok";
    case DBC_CATALOG_INDEX_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_CATALOG_INDEX_INVALID_STATE: return "invalid_state";
    case DBC_CATALOG_INDEX_IO_ERROR: return "io_error";
    case DBC_CATALOG_INDEX_LIMIT_EXCEEDED: return "limit_exceeded";
    case DBC_CATALOG_INDEX_DUPLICATE_MESSAGE: return "duplicate_message";
    case DBC_CATALOG_INDEX_DUPLICATE_KEY: return "duplicate_key";
    case DBC_CATALOG_INDEX_INVALID_RECORD: return "invalid_record";
    case DBC_CATALOG_INDEX_INVALID_FORMAT: return "invalid_format";
    case DBC_CATALOG_INDEX_CRC_MISMATCH: return "crc_mismatch";
    case DBC_CATALOG_INDEX_SOURCE_MISMATCH: return "source_mismatch";
    default: return "unknown";
  }
}
