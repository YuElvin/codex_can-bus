#ifndef DBC_CANDIDATE_HTTP_H
#define DBC_CANDIDATE_HTTP_H

#include <stddef.h>
#include <stdint.h>

#include "dbc_candidate_catalog.h"
#include "dbc_candidate_commit.h"

#define DBC_CANDIDATE_HTTP_QUERY_STORAGE_BYTES \
  (LARGE_DBC_QUERY_MAX_BYTES + 1u)

typedef enum {
  DBC_CANDIDATE_HTTP_OK = 0,
  DBC_CANDIDATE_HTTP_INVALID_ARGUMENT,
  DBC_CANDIDATE_HTTP_INVALID_TARGET,
  DBC_CANDIDATE_HTTP_INVALID_ENCODING,
  DBC_CANDIDATE_HTTP_UNKNOWN_FIELD,
  DBC_CANDIDATE_HTTP_DUPLICATE_FIELD,
  DBC_CANDIDATE_HTTP_INVALID_NUMBER,
  DBC_CANDIDATE_HTTP_INVALID_TOKEN,
  DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED,
  DBC_CANDIDATE_HTTP_MUTATION_CONFLICT,
  DBC_CANDIDATE_HTTP_INVALID_STATE,
  DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED
} DbcCandidateHttpStatus;

/*
 * Accepts either the exact GET target /api/dbc/candidate/signals[?query] or a
 * query string with an optional leading '?'.  page defaults to zero, q to an
 * empty string, selected to all, and pageSize is always eight.
 */
DbcCandidateHttpStatus dbc_candidate_http_parse_get_target(
  const char *target,
  size_t target_length,
  DbcCandidateCatalogQuery *query,
  char query_storage[DBC_CANDIDATE_HTTP_QUERY_STORAGE_BYTES]);

typedef struct {
  char candidate_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  uint16_t set_ordinals[LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS];
  uint16_t clear_ordinals[LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS];
  uint8_t set_count;
  uint8_t clear_count;
} DbcCandidateHttpSelectionForm;

DbcCandidateHttpStatus dbc_candidate_http_parse_selection_form(
  const char *body,
  size_t body_length,
  DbcCandidateHttpSelectionForm *form);

DbcCandidateHttpStatus dbc_candidate_http_serialize_catalog_json(
  const char *candidate_token,
  const DbcCandidateDescriptor *descriptor,
  const DbcCandidateCatalogPage *page,
  char *output,
  size_t output_capacity,
  size_t *written);

DbcCandidateHttpStatus dbc_candidate_http_serialize_selection_json(
  const char *candidate_token,
  const DbcCandidateDescriptor *descriptor,
  char *output,
  size_t output_capacity,
  size_t *written);

const char *dbc_candidate_http_status_string(DbcCandidateHttpStatus status);

#endif
