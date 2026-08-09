#ifndef SIGNAL_API_H
#define SIGNAL_API_H

#include <stddef.h>
#include <stdint.h>

#include "dbc_selected_runtime.h"
#include "large_dbc_contract.h"
#include "signal_cache.h"

#define SIGNAL_API_MAX_ITEMS 2u

typedef enum {
  SIGNAL_API_OK = 0,
  SIGNAL_API_INVALID_ARGUMENT,
  SIGNAL_API_INVALID_TARGET,
  SIGNAL_API_UNKNOWN_FIELD,
  SIGNAL_API_DUPLICATE_FIELD,
  SIGNAL_API_INVALID_PAGE,
  SIGNAL_API_INVALID_QUERY,
  SIGNAL_API_INVALID_STATE,
  SIGNAL_API_SNAPSHOT_BUSY,
  SIGNAL_API_CAPACITY_EXCEEDED
} SignalApiStatus;

typedef struct {
  uint32_t page;
  char query[LARGE_DBC_QUERY_MAX_BYTES + 1u];
} SignalApiQuery;

typedef struct {
  const DbcSelectedRuntimeSignal *signal;
  SignalValueSnapshot value;
} SignalApiPageItem;

typedef struct {
  uint64_t generation;
  uint32_t selection_crc32;
  uint32_t page;
  uint16_t total;
  uint16_t matched;
  uint8_t page_size;
  uint8_t item_count;
  SignalApiPageItem items[LARGE_DBC_API_PAGE_ITEMS];
} SignalApiPage;

_Static_assert(sizeof(SignalApiPage) <= LARGE_DBC_MAX_AUTOMATIC_OBJECT_BYTES,
               "signal API page must remain a small automatic snapshot");

SignalApiStatus signal_api_parse_get_target(const char *target,
                                             size_t target_length,
                                             SignalApiQuery *query);
SignalApiStatus signal_api_build_selected_page(
  const DbcSelectedRuntimeSnapshot *snapshot,
  const SignalApiQuery *query,
  uint32_t now_ms,
  uint32_t stale_after_ms,
  SignalApiPage *page);
SignalApiStatus signal_api_serialize_selected_page(const SignalApiPage *page,
                                                   char *body,
                                                   size_t body_len,
                                                   size_t *written);
const char *signal_api_status_string(SignalApiStatus status);

size_t signal_api_build_json(const SignalCacheEntry *entries,
                             size_t entry_count,
                             char *body,
                             size_t body_len);

#endif
