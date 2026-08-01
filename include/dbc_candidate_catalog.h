#ifndef DBC_CANDIDATE_CATALOG_H
#define DBC_CANDIDATE_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dbc_candidate_format.h"
#include "dbc_catalog_index.h"

typedef enum {
  DBC_CANDIDATE_CATALOG_OK = 0,
  DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT,
  DBC_CANDIDATE_CATALOG_INVALID_STATE,
  DBC_CANDIDATE_CATALOG_IO_FAILED,
  DBC_CANDIDATE_CATALOG_TOKEN_MISMATCH,
  DBC_CANDIDATE_CATALOG_WRITE_BLOCKED,
  DBC_CANDIDATE_CATALOG_MUTATION_CONFLICT,
  DBC_CANDIDATE_CATALOG_ORDINAL_OUT_OF_RANGE,
  DBC_CANDIDATE_CATALOG_SELECTION_LIMIT,
  DBC_CANDIDATE_CATALOG_MESSAGE_LIMIT,
  DBC_CANDIDATE_CATALOG_GENERATION_EXHAUSTED
} DbcCandidateCatalogStatus;

typedef enum {
  DBC_CANDIDATE_FILTER_ALL = 0,
  DBC_CANDIDATE_FILTER_SELECTED,
  DBC_CANDIDATE_FILTER_UNSELECTED
} DbcCandidateSelectionFilter;

typedef struct {
  uint16_t ordinal;
  bool selected;
  char key[LARGE_DBC_SIGNAL_RECORD_KEY_BYTES];
} DbcCandidateCatalogItem;

typedef struct {
  uint32_t page;
  uint8_t page_size;
  const char *query;
  size_t query_length;
  DbcCandidateSelectionFilter selected_filter;
} DbcCandidateCatalogQuery;

typedef struct {
  uint16_t catalog_total;
  uint16_t matched_total;
  uint32_t page;
  uint8_t page_size;
  uint8_t item_count;
  bool has_more;
  DbcCandidateCatalogItem items[LARGE_DBC_API_PAGE_ITEMS];
} DbcCandidateCatalogPage;

typedef struct {
  DbcCatalogIndexSignal signal;
  union {
    uint8_t selection_bytes[LARGE_DBC_SELECTION_TOTAL_SIZE];
    DbcSelectionV1 selection;
  } scratch;
} DbcCandidateCatalogWorkspace;

DbcCandidateCatalogStatus dbc_candidate_catalog_query(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  const DbcSelectionV1 *selection,
  const DbcCandidateCatalogQuery *query,
  DbcCandidateCatalogWorkspace *workspace,
  DbcCandidateCatalogPage *page);

typedef struct {
  const char *candidate_token;
  const uint16_t *set_ordinals;
  size_t set_count;
  const uint16_t *clear_ordinals;
  size_t clear_count;
  bool writes_blocked;
} DbcCandidateSelectionMutation;

typedef struct {
  DbcManifestV1 manifest;
  DbcSelectionV1 selection;
  char candidate_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
} DbcCandidateSelectionUpdate;

/*
 * Produces a new generation in memory only.  The caller persists the copied
 * source/index, encoded selection, and manifest through the C transaction
 * backend; this function performs no filesystem/runtime/active mutation.
 */
DbcCandidateCatalogStatus dbc_candidate_selection_prepare_update(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  const DbcManifestV1 *current_manifest,
  const DbcSelectionV1 *current_selection,
  const DbcCandidateSelectionMutation *mutation,
  DbcCandidateCatalogWorkspace *workspace,
  DbcCandidateSelectionUpdate *update);

const char *dbc_candidate_catalog_status_string(
  DbcCandidateCatalogStatus status);

#endif
