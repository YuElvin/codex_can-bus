#ifndef DBC_SIGNAL_CATALOG_H
#define DBC_SIGNAL_CATALOG_H

#include <stdbool.h>
#include <stddef.h>

#include "dbc_parser.h"
#include "rule_engine.h"

#define DBC_SIGNAL_CATALOG_PAGE_SIZE 16u

typedef struct {
  char key[RULE_SIGNAL_KEY_MAX];
} DbcSignalCatalogEntry;

size_t dbc_signal_catalog_total(const DbcDatabase *db);
bool dbc_signal_catalog_contains(const DbcDatabase *db, const char *key);
size_t dbc_signal_catalog_page(const DbcDatabase *db,
                               size_t page,
                               DbcSignalCatalogEntry *out_entries,
                               size_t out_capacity);

#endif
