#include "dbc_signal_catalog.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_paging_and_key_validation(void) {
  DbcDatabase db;
  DbcSignalCatalogEntry page[DBC_SIGNAL_CATALOG_PAGE_SIZE];

  dbc_init(&db);
  db.message_count = 1u;
  (void)snprintf(db.messages[0].name, sizeof(db.messages[0].name), "EngineData");
  db.messages[0].signal_start = 0u;
  db.messages[0].signal_count = 18u;
  db.signal_count = 18u;
  for (size_t i = 0u; i < db.signal_count; ++i) {
    (void)snprintf(db.signals[i].name, sizeof(db.signals[i].name), "signal%u", (unsigned)i);
  }

  ASSERT_TRUE(dbc_signal_catalog_total(&db) == 18u);
  ASSERT_TRUE(dbc_signal_catalog_contains(&db, "EngineData.signal0"));
  ASSERT_TRUE(!dbc_signal_catalog_contains(&db, "EngineData.missing"));
  ASSERT_TRUE(dbc_signal_catalog_page(&db, 0u, page, DBC_SIGNAL_CATALOG_PAGE_SIZE) == 16u);
  ASSERT_TRUE(strcmp(page[0].key, "EngineData.signal0") == 0);
  ASSERT_TRUE(strcmp(page[15].key, "EngineData.signal15") == 0);
  ASSERT_TRUE(dbc_signal_catalog_page(&db, 1u, page, DBC_SIGNAL_CATALOG_PAGE_SIZE) == 2u);
  ASSERT_TRUE(strcmp(page[0].key, "EngineData.signal16") == 0);
  ASSERT_TRUE(dbc_signal_catalog_page(&db, 2u, page, DBC_SIGNAL_CATALOG_PAGE_SIZE) == 0u);
  return 0;
}

int main(void) {
  return test_paging_and_key_validation();
}
