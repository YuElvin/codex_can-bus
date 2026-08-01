#ifndef DBC_CATALOG_INDEX_H
#define DBC_CATALOG_INDEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "large_dbc_contract.h"

/*
 * Random-access byte I/O used by both host FILE adapters and FatFs adapters.
 * Implementations must either transfer the complete requested range or fail.
 */
typedef bool (*DbcCatalogIndexReadAt)(void *context,
                                     uint32_t offset,
                                     uint8_t *data,
                                     uint32_t size);
typedef bool (*DbcCatalogIndexWriteAt)(void *context,
                                      uint32_t offset,
                                      const uint8_t *data,
                                      uint32_t size);
typedef bool (*DbcCatalogIndexResize)(void *context, uint32_t size);
typedef bool (*DbcCatalogIndexGetSize)(void *context, uint32_t *size);

typedef struct {
  void *context;
  DbcCatalogIndexReadAt read_at;
  DbcCatalogIndexWriteAt write_at;
  DbcCatalogIndexResize resize;
  DbcCatalogIndexGetSize get_size;
} DbcCatalogIndexIo;

typedef enum {
  DBC_CATALOG_INDEX_OK = 0,
  DBC_CATALOG_INDEX_INVALID_ARGUMENT,
  DBC_CATALOG_INDEX_INVALID_STATE,
  DBC_CATALOG_INDEX_IO_ERROR,
  DBC_CATALOG_INDEX_LIMIT_EXCEEDED,
  DBC_CATALOG_INDEX_DUPLICATE_MESSAGE,
  DBC_CATALOG_INDEX_DUPLICATE_KEY,
  DBC_CATALOG_INDEX_INVALID_RECORD,
  DBC_CATALOG_INDEX_INVALID_FORMAT,
  DBC_CATALOG_INDEX_CRC_MISMATCH,
  DBC_CATALOG_INDEX_SOURCE_MISMATCH
} DbcCatalogIndexStatus;

typedef struct {
  uint32_t normalized_id;
  uint32_t source_line;
  uint8_t declared_payload_length;
  bool ide;
} DbcCatalogIndexMessageInput;

typedef struct {
  uint16_t start_bit;
  uint8_t bit_length;
  bool motorola;
  bool is_signed;
  double factor;
  double offset;
  double minimum;
  double maximum;
  uint64_t definition_hash;
  uint32_t source_line;
  uint32_t source_offset;
  const char *key;
  uint8_t key_length;
  const char *unit;
  uint8_t unit_length;
} DbcCatalogIndexSignalInput;

typedef struct {
  uint16_t ordinal;
  uint32_t normalized_id;
  uint32_t source_line;
  uint16_t first_signal_ordinal;
  uint16_t signal_count;
  uint8_t flags;
  uint8_t declared_payload_length;
} DbcCatalogIndexMessage;

typedef struct {
  uint16_t ordinal;
  uint16_t message_ordinal;
  uint32_t normalized_id;
  uint8_t flags;
  uint16_t start_bit;
  uint8_t bit_length;
  uint8_t declared_payload_length;
  double factor;
  double offset;
  double minimum;
  double maximum;
  uint64_t definition_hash;
  uint32_t source_line;
  uint32_t source_offset;
  char key[LARGE_DBC_SIGNAL_RECORD_KEY_BYTES];
  char unit[LARGE_DBC_SIGNAL_RECORD_UNIT_BYTES];
} DbcCatalogIndexSignal;

typedef struct {
  uint32_t source_size;
  uint32_t source_crc32;
  uint16_t message_count;
  uint16_t signal_count;
  uint32_t total_size;
  uint32_t message_records_crc32;
  uint32_t signal_records_crc32;
} DbcCatalogIndexSummary;

typedef struct {
  bool enabled;
  uint32_t source_size;
  uint32_t source_crc32;
} DbcCatalogIndexExpectedSource;

/*
 * Two 8192-bit Bloom filters gate exact spool/index readback. Bloom false
 * positives only cost I/O; every possible hit is compared byte-for-byte, so
 * duplicate rejection has no false positives or false negatives.
 */
#define DBC_CATALOG_INDEX_BLOOM_BYTES 1024u

typedef struct {
  uint8_t message_bits[DBC_CATALOG_INDEX_BLOOM_BYTES];
  uint8_t signal_bits[DBC_CATALOG_INDEX_BLOOM_BYTES];
} DbcCatalogIndexVerifyWorkspace;

typedef struct {
  DbcCatalogIndexIo message_spool;
  DbcCatalogIndexIo signal_spool;
  uint32_t source_size;
  uint32_t source_crc32;
  uint16_t message_count;
  uint16_t signal_count;
  uint16_t current_message_ordinal;
  uint16_t current_message_signal_count;
  DbcCatalogIndexMessageInput current_message;
  bool has_current_message;
  bool initialized;
  bool finalized;
  DbcCatalogIndexVerifyWorkspace duplicate_workspace;
} DbcCatalogIndexBuilder;

uint32_t dbc_catalog_crc32_begin(void);
uint32_t dbc_catalog_crc32_update(uint32_t state,
                                  const uint8_t *data,
                                  size_t size);
uint32_t dbc_catalog_crc32_finish(uint32_t state);
uint64_t dbc_catalog_definition_hash_v1(
  const DbcCatalogIndexMessageInput *message,
  const DbcCatalogIndexSignalInput *signal);

DbcCatalogIndexStatus dbc_catalog_index_builder_init(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexIo *message_spool,
  const DbcCatalogIndexIo *signal_spool,
  uint32_t source_size,
  uint32_t source_crc32);
DbcCatalogIndexStatus dbc_catalog_index_builder_begin_message(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexMessageInput *message);
DbcCatalogIndexStatus dbc_catalog_index_builder_add_signal(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexSignalInput *signal);
DbcCatalogIndexStatus dbc_catalog_index_builder_finalize(
  DbcCatalogIndexBuilder *builder,
  const DbcCatalogIndexIo *output,
  DbcCatalogIndexSummary *summary);

DbcCatalogIndexStatus dbc_catalog_index_verify(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexExpectedSource *expected_source,
  DbcCatalogIndexVerifyWorkspace *workspace,
  DbcCatalogIndexSummary *summary);
DbcCatalogIndexStatus dbc_catalog_index_read_message(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  uint16_t ordinal,
  DbcCatalogIndexMessage *message);
DbcCatalogIndexStatus dbc_catalog_index_read_signal(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  uint16_t ordinal,
  DbcCatalogIndexSignal *signal);

typedef bool (*DbcCatalogIndexTextWrite)(void *context,
                                        const char *text,
                                        size_t size);
DbcCatalogIndexStatus dbc_catalog_index_dump(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *verified_summary,
  DbcCatalogIndexTextWrite write_text,
  void *write_context);

const char *dbc_catalog_index_status_string(DbcCatalogIndexStatus status);

#endif
