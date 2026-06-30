#include "signal_cache.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_EQ_SIZE(expected, actual)                                                     \
  do {                                                                                       \
    if ((expected) != (actual)) {                                                            \
      printf("ASSERT_EQ_SIZE failed at %s:%d: expected %zu got %zu\n",                       \
             __FILE__,                                                                       \
             __LINE__,                                                                       \
             (size_t)(expected),                                                             \
             (size_t)(actual));                                                              \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_upsert_and_find(void) {
  SignalCache cache;
  signal_cache_init(&cache);

  ASSERT_TRUE(signal_cache_upsert(&cache, "EngineData", "rpm", "rpm", 1200.0, 9600, 10u));
  ASSERT_EQ_SIZE(1u, cache.count);

  const SignalCacheEntry *entry = signal_cache_find(&cache, "EngineData.rpm");
  ASSERT_TRUE(entry != NULL);
  ASSERT_TRUE(strcmp(entry->unit, "rpm") == 0);
  ASSERT_TRUE(entry->physical_value == 1200.0);
  ASSERT_TRUE(entry->raw_value == 9600);
  ASSERT_TRUE(entry->quality == SIGNAL_QUALITY_OK);

  ASSERT_TRUE(signal_cache_upsert(&cache, "EngineData", "rpm", "rpm", 1300.0, 10400, 20u));
  ASSERT_EQ_SIZE(1u, cache.count);
  entry = signal_cache_find(&cache, "EngineData.rpm");
  ASSERT_TRUE(entry != NULL);
  ASSERT_TRUE(entry->physical_value == 1300.0);
  ASSERT_TRUE(entry->updated_ms == 20u);
  return 0;
}

static int test_stale_and_rule_snapshot_export(void) {
  SignalCache cache;
  signal_cache_init(&cache);

  ASSERT_TRUE(signal_cache_upsert(&cache, "EngineData", "rpm", "rpm", 1200.0, 9600, 10u));
  ASSERT_TRUE(signal_cache_upsert(&cache, "EngineData", "temp", "degC", 80.0, 120, 70u));
  signal_cache_mark_stale(&cache, 120u, 100u);

  SignalSnapshot snapshots[4];
  const size_t count = signal_cache_export_rule_snapshots(&cache, snapshots, 4u);
  ASSERT_EQ_SIZE(2u, count);
  ASSERT_TRUE(strcmp(snapshots[0].key, "EngineData.rpm") == 0);
  ASSERT_TRUE(!snapshots[0].valid);
  ASSERT_TRUE(strcmp(snapshots[1].key, "EngineData.temp") == 0);
  ASSERT_TRUE(snapshots[1].valid);
  ASSERT_TRUE(snapshots[1].value == 80.0);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_upsert_and_find() == 0);
  ASSERT_TRUE(test_stale_and_rule_snapshot_export() == 0);
  return 0;
}
