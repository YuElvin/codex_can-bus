#include "dbc_signal_catalog.h"

#include <stdio.h>
#include <string.h>

static bool dbc_signal_catalog_key(const DbcDatabase *db, size_t signal_index, char *key, size_t key_len) {
  if (db == NULL || key == NULL || signal_index >= db->signal_count) {
    return false;
  }
  for (size_t message_index = 0u; message_index < db->message_count; ++message_index) {
    const DbcMessage *message = &db->messages[message_index];
    if (signal_index < message->signal_start ||
        signal_index >= message->signal_start + message->signal_count) {
      continue;
    }
    return snprintf(key, key_len, "%s.%s", message->name, db->signals[signal_index].name) <
           (int)key_len;
  }
  return false;
}

size_t dbc_signal_catalog_total(const DbcDatabase *db) {
  size_t total = 0u;
  char key[RULE_SIGNAL_KEY_MAX];

  if (db == NULL) {
    return 0u;
  }
  for (size_t signal_index = 0u; signal_index < db->signal_count; ++signal_index) {
    if (dbc_signal_catalog_key(db, signal_index, key, sizeof(key))) {
      ++total;
    }
  }
  return total;
}

bool dbc_signal_catalog_contains(const DbcDatabase *db, const char *key) {
  char candidate[RULE_SIGNAL_KEY_MAX];

  if (db == NULL || key == NULL || key[0] == '\0') {
    return false;
  }
  for (size_t signal_index = 0u; signal_index < db->signal_count; ++signal_index) {
    if (dbc_signal_catalog_key(db, signal_index, candidate, sizeof(candidate)) &&
        strcmp(candidate, key) == 0) {
      return true;
    }
  }
  return false;
}

size_t dbc_signal_catalog_page(const DbcDatabase *db,
                               size_t page,
                               DbcSignalCatalogEntry *out_entries,
                               size_t out_capacity) {
  size_t first;
  size_t ordinal = 0u;
  size_t written = 0u;

  if (db == NULL || out_entries == NULL || out_capacity == 0u) {
    return 0u;
  }
  if (page > SIZE_MAX / DBC_SIGNAL_CATALOG_PAGE_SIZE) {
    return 0u;
  }
  first = page * DBC_SIGNAL_CATALOG_PAGE_SIZE;
  for (size_t signal_index = 0u; signal_index < db->signal_count; ++signal_index) {
    char key[RULE_SIGNAL_KEY_MAX];
    if (!dbc_signal_catalog_key(db, signal_index, key, sizeof(key))) {
      continue;
    }
    if (ordinal >= first && written < out_capacity) {
      (void)snprintf(out_entries[written].key, sizeof(out_entries[written].key), "%s", key);
      ++written;
    }
    ++ordinal;
    if (written == out_capacity) {
      break;
    }
  }
  return written;
}
