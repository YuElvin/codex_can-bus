#include "dbc_candidate_catalog.h"

#include <string.h>

_Static_assert(LARGE_DBC_API_PAGE_ITEMS == 8u,
               "candidate catalog page contract changed");
_Static_assert(sizeof(DbcCandidateCatalogWorkspace) <=
                 LARGE_DBC_MAX_AUTOMATIC_OBJECT_BYTES,
               "catalog workspace unexpectedly grew");

static uint8_t ascii_fold(uint8_t value) {
  return value >= (uint8_t)'A' && value <= (uint8_t)'Z' ?
    (uint8_t)(value + ((uint8_t)'a' - (uint8_t)'A')) : value;
}

static bool query_matches(const char *key, const char *query,
                          size_t query_length) {
  if (query_length == 0u) {
    return true;
  }
  const size_t key_length = strlen(key);
  if (query_length > key_length) {
    return false;
  }
  for (size_t start = 0u; start <= key_length - query_length; ++start) {
    bool equal = true;
    for (size_t i = 0u; i < query_length; ++i) {
      if (ascii_fold((uint8_t)key[start + i]) !=
          ascii_fold((uint8_t)query[i])) {
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

static bool filter_matches(DbcCandidateSelectionFilter filter, bool selected) {
  switch (filter) {
    case DBC_CANDIDATE_FILTER_ALL: return true;
    case DBC_CANDIDATE_FILTER_SELECTED: return selected;
    case DBC_CANDIDATE_FILTER_UNSELECTED: return !selected;
    default: return false;
  }
}

static bool selection_identity_valid(const DbcCatalogIndexSummary *summary,
                                     const DbcSelectionV1 *selection,
                                     DbcCandidateCatalogWorkspace *workspace) {
  return summary != NULL && selection != NULL && workspace != NULL &&
         selection->candidate_generation != 0u &&
         selection->selection_generation != 0u &&
         selection->source_size == summary->source_size &&
         selection->source_crc32 == summary->source_crc32 &&
         selection->catalog_signal_count == summary->signal_count &&
         dbc_selection_v1_encode(selection,
                                 workspace->scratch.selection_bytes) ==
           DBC_CANDIDATE_FORMAT_OK;
}

DbcCandidateCatalogStatus dbc_candidate_catalog_query(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  const DbcSelectionV1 *selection,
  const DbcCandidateCatalogQuery *query,
  DbcCandidateCatalogWorkspace *workspace,
  DbcCandidateCatalogPage *page) {
  if (index == NULL || summary == NULL || selection == NULL || query == NULL ||
      workspace == NULL || page == NULL || query->page_size == 0u ||
      query->page_size > LARGE_DBC_API_PAGE_ITEMS ||
      query->query_length > LARGE_DBC_QUERY_MAX_BYTES ||
      (query->query_length != 0u && query->query == NULL) ||
      query->selected_filter > DBC_CANDIDATE_FILTER_UNSELECTED) {
    return DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT;
  }
  for (size_t i = 0u; i < query->query_length; ++i) {
    if (query->query[i] == '\0') {
      return DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT;
    }
  }
  if (!selection_identity_valid(summary, selection, workspace)) {
    return DBC_CANDIDATE_CATALOG_INVALID_STATE;
  }

  memset(page, 0, sizeof(*page));
  page->catalog_total = summary->signal_count;
  page->page = query->page;
  page->page_size = query->page_size;
  const uint64_t skip = (uint64_t)query->page * query->page_size;
  const bool matched_total_known = query->query_length == 0u;
  if (matched_total_known) {
    switch (query->selected_filter) {
      case DBC_CANDIDATE_FILTER_ALL:
        page->matched_total = summary->signal_count;
        break;
      case DBC_CANDIDATE_FILTER_SELECTED:
        page->matched_total = selection->selected_count;
        break;
      case DBC_CANDIDATE_FILTER_UNSELECTED:
        page->matched_total =
          (uint16_t)(summary->signal_count - selection->selected_count);
        break;
      default:
        return DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT;
    }
  }
  uint32_t matched = 0u;
  for (uint16_t ordinal = 0u; ordinal < summary->signal_count; ++ordinal) {
    const bool selected = dbc_selection_v1_is_selected(selection, ordinal);
    if (!filter_matches(query->selected_filter, selected)) {
      continue;
    }
    const bool page_needs_item = (uint64_t)matched >= skip &&
                                 page->item_count < query->page_size;
    if ((query->query_length != 0u || page_needs_item) &&
        (dbc_catalog_index_read_signal(index, summary, ordinal,
                                       &workspace->signal) !=
           DBC_CATALOG_INDEX_OK ||
         workspace->signal.ordinal != ordinal ||
         workspace->signal.key[0] == '\0')) {
      return DBC_CANDIDATE_CATALOG_IO_FAILED;
    }
    if (query->query_length != 0u &&
        !query_matches(workspace->signal.key, query->query,
                       query->query_length)) {
      continue;
    }
    if (page_needs_item) {
      DbcCandidateCatalogItem *item = &page->items[page->item_count++];
      item->ordinal = ordinal;
      item->selected = selected;
      memcpy(item->key, workspace->signal.key, sizeof(item->key));
    }
    ++matched;
    if (matched_total_known && page->item_count == query->page_size) {
      break;
    }
  }
  if (!matched_total_known) {
    page->matched_total = (uint16_t)matched;
  }
  page->has_more = skip + page->item_count < page->matched_total;
  return DBC_CANDIDATE_CATALOG_OK;
}

static bool index_matches_manifest(const DbcCatalogIndexSummary *summary,
                                   const DbcManifestV1 *manifest) {
  return summary->source_size == manifest->source_size &&
         summary->source_crc32 == manifest->source_crc32 &&
         summary->total_size == manifest->index_size &&
         summary->message_count == manifest->catalog_message_count &&
         summary->signal_count == manifest->catalog_signal_count;
}

static __attribute__((noinline)) DbcCandidateCatalogStatus count_messages(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  const DbcSelectionV1 *selection,
  DbcCandidateCatalogWorkspace *workspace,
  uint16_t *message_count) {
  uint16_t count = 0u;
  uint16_t last_message = UINT16_MAX;
  for (uint16_t ordinal = 0u; ordinal < summary->signal_count; ++ordinal) {
    if (!dbc_selection_v1_is_selected(selection, ordinal)) {
      continue;
    }
    if (dbc_catalog_index_read_signal(index, summary, ordinal,
                                      &workspace->signal) !=
        DBC_CATALOG_INDEX_OK) {
      return DBC_CANDIDATE_CATALOG_IO_FAILED;
    }
    if (workspace->signal.message_ordinal != last_message) {
      ++count;
      last_message = workspace->signal.message_ordinal;
    }
  }
  *message_count = count;
  return DBC_CANDIDATE_CATALOG_OK;
}

static bool contains_ordinal(const uint16_t *ordinals, size_t count,
                             uint16_t ordinal) {
  for (size_t i = 0u; i < count; ++i) {
    if (ordinals[i] == ordinal) {
      return true;
    }
  }
  return false;
}

static DbcCandidateCatalogStatus validate_mutation(
  const DbcCandidateSelectionMutation *mutation,
  uint16_t signal_count) {
  if ((mutation->set_count == 0u && mutation->clear_count == 0u) ||
      mutation->set_count > LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS ||
      mutation->clear_count > LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS ||
      mutation->set_count > LARGE_DBC_SELECTION_UPDATE_MAX_ORDINALS -
                              mutation->clear_count ||
      (mutation->set_count != 0u && mutation->set_ordinals == NULL) ||
      (mutation->clear_count != 0u && mutation->clear_ordinals == NULL)) {
    return DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT;
  }
  for (size_t i = 0u; i < mutation->set_count; ++i) {
    const uint16_t ordinal = mutation->set_ordinals[i];
    if (ordinal >= signal_count) {
      return DBC_CANDIDATE_CATALOG_ORDINAL_OUT_OF_RANGE;
    }
    if (contains_ordinal(mutation->set_ordinals, i, ordinal) ||
        contains_ordinal(mutation->clear_ordinals,
                         mutation->clear_count, ordinal)) {
      return DBC_CANDIDATE_CATALOG_MUTATION_CONFLICT;
    }
  }
  for (size_t i = 0u; i < mutation->clear_count; ++i) {
    const uint16_t ordinal = mutation->clear_ordinals[i];
    if (ordinal >= signal_count) {
      return DBC_CANDIDATE_CATALOG_ORDINAL_OUT_OF_RANGE;
    }
    if (contains_ordinal(mutation->clear_ordinals, i, ordinal)) {
      return DBC_CANDIDATE_CATALOG_MUTATION_CONFLICT;
    }
  }
  return DBC_CANDIDATE_CATALOG_OK;
}

static uint16_t bitmap_count(const DbcSelectionV1 *selection) {
  uint16_t count = 0u;
  for (uint16_t ordinal = 0u; ordinal < selection->catalog_signal_count;
       ++ordinal) {
    if (dbc_selection_v1_is_selected(selection, ordinal)) {
      ++count;
    }
  }
  return count;
}

DbcCandidateCatalogStatus dbc_candidate_selection_prepare_update(
  const DbcCatalogIndexIo *index,
  const DbcCatalogIndexSummary *summary,
  const DbcManifestV1 *current_manifest,
  const DbcSelectionV1 *current_selection,
  const DbcCandidateSelectionMutation *mutation,
  DbcCandidateCatalogWorkspace *workspace,
  DbcCandidateSelectionUpdate *update) {
  if (index == NULL || summary == NULL || current_manifest == NULL ||
      current_selection == NULL || mutation == NULL || workspace == NULL ||
      update == NULL || mutation->candidate_token == NULL) {
    return DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT;
  }
  if (mutation->writes_blocked) {
    return DBC_CANDIDATE_CATALOG_WRITE_BLOCKED;
  }
  if (current_manifest->object_kind != LARGE_DBC_MANIFEST_OBJECT_CANDIDATE ||
      dbc_candidate_token_verify_manifest(mutation->candidate_token,
                                          current_manifest) !=
        DBC_CANDIDATE_FORMAT_OK) {
    return DBC_CANDIDATE_CATALOG_TOKEN_MISMATCH;
  }
  if (!index_matches_manifest(summary, current_manifest) ||
      !selection_identity_valid(summary, current_selection, workspace) ||
      current_selection->candidate_generation != current_manifest->generation ||
      current_selection->selected_count != current_manifest->selected_count ||
      dbc_candidate_crc32(workspace->scratch.selection_bytes,
                          sizeof(workspace->scratch.selection_bytes)) !=
        current_manifest->selection_crc32) {
    return DBC_CANDIDATE_CATALOG_INVALID_STATE;
  }
  uint16_t current_messages = 0u;
  DbcCandidateCatalogStatus status = count_messages(
    index, summary, current_selection, workspace, &current_messages);
  if (status != DBC_CANDIDATE_CATALOG_OK) {
    return status;
  }
  const DbcCandidateIndexFacts index_facts = {
    .source_size = current_manifest->source_size,
    .source_crc32 = current_manifest->source_crc32,
    .index_size = current_manifest->index_size,
    .index_crc32 = current_manifest->index_crc32,
    .catalog_message_count = current_manifest->catalog_message_count,
    .catalog_signal_count = current_manifest->catalog_signal_count
  };
  if (current_messages != current_manifest->selected_message_count ||
      dbc_candidate_v1_verify_set(current_manifest, &index_facts,
                                  workspace->scratch.selection_bytes,
                                  sizeof(workspace->scratch.selection_bytes),
                                  current_messages, NULL) !=
        DBC_CANDIDATE_FORMAT_OK) {
    return DBC_CANDIDATE_CATALOG_INVALID_STATE;
  }
  status = validate_mutation(mutation, summary->signal_count);
  if (status != DBC_CANDIDATE_CATALOG_OK) {
    return status;
  }

  workspace->scratch.selection = *current_selection;
  for (size_t i = 0u; i < mutation->set_count; ++i) {
    const uint16_t ordinal = mutation->set_ordinals[i];
    workspace->scratch.selection.bitmap[ordinal >> 3u] |=
      (uint8_t)(1u << (ordinal & 7u));
  }
  for (size_t i = 0u; i < mutation->clear_count; ++i) {
    const uint16_t ordinal = mutation->clear_ordinals[i];
    workspace->scratch.selection.bitmap[ordinal >> 3u] &=
      (uint8_t)~(uint8_t)(1u << (ordinal & 7u));
  }
  workspace->scratch.selection.selected_count = bitmap_count(
    &workspace->scratch.selection);
  if (workspace->scratch.selection.selected_count >
        LARGE_DBC_ACTIVE_MAX_SIGNALS) {
    return DBC_CANDIDATE_CATALOG_SELECTION_LIMIT;
  }
  uint64_t next_generation = 0u;
  if (dbc_candidate_next_generation(current_manifest->generation,
                                    &next_generation) !=
      DBC_CANDIDATE_FORMAT_OK) {
    return DBC_CANDIDATE_CATALOG_GENERATION_EXHAUSTED;
  }
  workspace->scratch.selection.candidate_generation = next_generation;
  workspace->scratch.selection.selection_generation = next_generation;
  uint16_t selected_messages = 0u;
  status = count_messages(index, summary, &workspace->scratch.selection,
                          workspace, &selected_messages);
  if (status != DBC_CANDIDATE_CATALOG_OK) {
    return status;
  }
  if (selected_messages > LARGE_DBC_ACTIVE_MAX_MESSAGES) {
    return DBC_CANDIDATE_CATALOG_MESSAGE_LIMIT;
  }
  const DbcSelectionV1 prepared_selection = workspace->scratch.selection;
  if (dbc_selection_v1_encode(&prepared_selection,
                              workspace->scratch.selection_bytes) !=
      DBC_CANDIDATE_FORMAT_OK) {
    return DBC_CANDIDATE_CATALOG_INVALID_STATE;
  }

  DbcManifestV1 prepared_manifest = *current_manifest;
  prepared_manifest.generation = next_generation;
  prepared_manifest.selection_size = LARGE_DBC_SELECTION_TOTAL_SIZE;
  prepared_manifest.selection_crc32 = dbc_candidate_crc32(
    workspace->scratch.selection_bytes,
    sizeof(workspace->scratch.selection_bytes));
  prepared_manifest.selected_count = prepared_selection.selected_count;
  prepared_manifest.selected_message_count = selected_messages;
  char prepared_token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  if (dbc_candidate_v1_verify_set(&prepared_manifest, &index_facts,
                                  workspace->scratch.selection_bytes,
                                  sizeof(workspace->scratch.selection_bytes),
                                  selected_messages, NULL) !=
      DBC_CANDIDATE_FORMAT_OK ||
      dbc_candidate_token_format(next_generation,
                                 prepared_manifest.source_size,
                                 prepared_manifest.source_crc32,
                                 prepared_token) !=
        DBC_CANDIDATE_FORMAT_OK) {
    return DBC_CANDIDATE_CATALOG_INVALID_STATE;
  }
  update->manifest = prepared_manifest;
  update->selection = prepared_selection;
  memcpy(update->candidate_token, prepared_token, sizeof(prepared_token));
  return DBC_CANDIDATE_CATALOG_OK;
}

const char *dbc_candidate_catalog_status_string(
  DbcCandidateCatalogStatus status) {
  switch (status) {
    case DBC_CANDIDATE_CATALOG_OK: return "ok";
    case DBC_CANDIDATE_CATALOG_INVALID_ARGUMENT: return "invalid_argument";
    case DBC_CANDIDATE_CATALOG_INVALID_STATE: return "invalid_state";
    case DBC_CANDIDATE_CATALOG_IO_FAILED: return "io_failed";
    case DBC_CANDIDATE_CATALOG_TOKEN_MISMATCH: return "token_mismatch";
    case DBC_CANDIDATE_CATALOG_WRITE_BLOCKED: return "write_blocked";
    case DBC_CANDIDATE_CATALOG_MUTATION_CONFLICT: return "mutation_conflict";
    case DBC_CANDIDATE_CATALOG_ORDINAL_OUT_OF_RANGE:
      return "ordinal_out_of_range";
    case DBC_CANDIDATE_CATALOG_SELECTION_LIMIT: return "selection_limit";
    case DBC_CANDIDATE_CATALOG_MESSAGE_LIMIT: return "message_limit";
    case DBC_CANDIDATE_CATALOG_GENERATION_EXHAUSTED:
      return "generation_exhausted";
    default: return "unknown";
  }
}
