#include "large_dbc_contract.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition)                                                   \
  do {                                                                           \
    if (!(condition)) {                                                          \
      fprintf(stderr, "assertion failed: %s:%d: %s\n", __FILE__, __LINE__,     \
              #condition);                                                       \
      return 1;                                                                  \
    }                                                                            \
  } while (0)

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
  while (length-- > 0u) {
    crc ^= *data++;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
      crc = (crc >> 1u) ^ ((crc & 1u) != 0u ?
        LARGE_DBC_CRC32_REFLECTED_POLYNOMIAL : 0u);
    }
  }
  return crc;
}

static uint64_t fnv1a64(const uint8_t *data, size_t length) {
  uint64_t hash = LARGE_DBC_FNV1A64_OFFSET_BASIS;
  while (length-- > 0u) {
    hash ^= *data++;
    hash *= LARGE_DBC_FNV1A64_PRIME;
  }
  return hash;
}

int main(void) {
  ASSERT_TRUE(sizeof(uint8_t) == 1u);
  ASSERT_TRUE(sizeof(uint16_t) == 2u);
  ASSERT_TRUE(sizeof(uint32_t) == 4u);
  ASSERT_TRUE(sizeof(uint64_t) == 8u);
  ASSERT_TRUE(sizeof(double) == 8u);

  ASSERT_TRUE(LARGE_DBC_SOURCE_MAX_BYTES == 256u * 1024u);
  ASSERT_TRUE(LARGE_DBC_CATALOG_MAX_MESSAGES >= 112u);
  ASSERT_TRUE(LARGE_DBC_CATALOG_MAX_SIGNALS >= 896u);
  ASSERT_TRUE(LARGE_DBC_ACTIVE_MAX_MESSAGES == 64u);
  ASSERT_TRUE(LARGE_DBC_ACTIVE_MAX_SIGNALS == 128u);
  ASSERT_TRUE(LARGE_DBC_SELECTION_BITMAP_BYTES == 256u);

  ASSERT_TRUE(LARGE_DBC_MANIFEST_CRC32_OFFSET + 4u == LARGE_DBC_MANIFEST_SIZE);
  ASSERT_TRUE(LARGE_DBC_INDEX_HEADER_CRC32_OFFSET + 4u == LARGE_DBC_INDEX_HEADER_SIZE);
  ASSERT_TRUE(LARGE_DBC_MESSAGE_RECORD_DECLARED_LENGTH_OFFSET + 1u <=
              LARGE_DBC_INDEX_MESSAGE_RECORD_SIZE);
  ASSERT_TRUE(LARGE_DBC_SIGNAL_RECORD_KEY_OFFSET + LARGE_DBC_SIGNAL_RECORD_KEY_BYTES ==
              LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET);
  ASSERT_TRUE(LARGE_DBC_SIGNAL_RECORD_UNIT_OFFSET + LARGE_DBC_SIGNAL_RECORD_UNIT_BYTES ==
              LARGE_DBC_INDEX_SIGNAL_RECORD_SIZE);
  ASSERT_TRUE(LARGE_DBC_SELECTION_HEADER_CRC32_OFFSET + 4u ==
              LARGE_DBC_SELECTION_HEADER_SIZE);
  ASSERT_TRUE(LARGE_DBC_SELECTION_TOTAL_SIZE == 320u);

  ASSERT_TRUE(LARGE_DBC_KEY_MAX_BYTES + 1u == LARGE_DBC_SIGNAL_RECORD_KEY_BYTES);
  ASSERT_TRUE(LARGE_DBC_UNIT_MAX_BYTES + 1u == LARGE_DBC_SIGNAL_RECORD_UNIT_BYTES);
  ASSERT_TRUE(LARGE_DBC_API_PAGE_ITEMS == 8u);
  ASSERT_TRUE(LARGE_DBC_HTTP_REALTIME_ITEM_WORST_BYTES * LARGE_DBC_API_PAGE_ITEMS +
                LARGE_DBC_HTTP_ENVELOPE_WORST_BYTES <=
              LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES);
  ASSERT_TRUE(LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES + 1u ==
              LARGE_DBC_HTTP_JSON_BODY_BYTES);
  ASSERT_TRUE(LARGE_DBC_HTTP_RESPONSE_SEGMENT_BYTES <= 2048u);
  ASSERT_TRUE(LARGE_DBC_CANDIDATE_TOKEN_CHARS + 1u ==
              LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES);

  static const uint8_t crc_check[] = "123456789";
  uint32_t crc = crc32_update(LARGE_DBC_CRC32_INITIAL_VALUE,
                              crc_check,
                              sizeof(crc_check) - 1u) ^ LARGE_DBC_CRC32_FINAL_XOR;
  ASSERT_TRUE(crc == UINT32_C(0xCBF43926));

  static const uint8_t definition_golden[LARGE_DBC_DEFINITION_SERIALIZED_BYTES] = {
    0x01, 0x23, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x10,
    0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xb9, 0x3f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0xc0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0xc0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x6a, 0x40
  };
  ASSERT_TRUE(fnv1a64(definition_golden, sizeof(definition_golden)) ==
              LARGE_DBC_DEFINITION_HASH_GOLDEN_V1);

  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  int token_length = snprintf(token, sizeof(token),
                              "%016llX-%08lX-%08lX",
                              (unsigned long long)17u,
                              (unsigned long)100071u,
                              (unsigned long)UINT32_C(0x4B88D9CE));
  ASSERT_TRUE(token_length == (int)LARGE_DBC_CANDIDATE_TOKEN_CHARS);
  ASSERT_TRUE(strcmp(token, "0000000000000011-000186E7-4B88D9CE") == 0);

  ASSERT_TRUE(128u * 1000u <=
              LARGE_DBC_MAX_LOG_ROWS_PER_SECOND * 10000u);
  ASSERT_TRUE(128u * 1000u >
              LARGE_DBC_MAX_LOG_ROWS_PER_SECOND * 1000u);
  ASSERT_TRUE(LARGE_DBC_FLASH_MAX_USED_BYTES + LARGE_DBC_FLASH_RESERVE_BYTES ==
              LARGE_DBC_FLASH_TOTAL_BYTES);
  ASSERT_TRUE(LARGE_DBC_NEW_TASK_COUNT == 0u);
  ASSERT_TRUE(LARGE_DBC_FREERTOS_HEAP_INCREMENT_BUDGET_BYTES == 0u);
  ASSERT_TRUE(LARGE_DBC_FREERTOS_MIN_EVER_FREE_HEAP_BYTES >= 4096u);

  puts("large DBC P0 contract tests passed");
  return 0;
}
