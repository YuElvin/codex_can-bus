#ifndef SIGNAL_CACHE_H
#define SIGNAL_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rule_engine.h"

#define SIGNAL_CACHE_MAX_VALUES 128u
#define SIGNAL_CACHE_KEY_MAX 48u
#define SIGNAL_CACHE_UNIT_MAX 16u
#define SIGNAL_CACHE_MESSAGE_MAX 32u
#define SIGNAL_CACHE_SIGNAL_MAX 32u

typedef enum {
  SIGNAL_QUALITY_MISSING = 0,
  SIGNAL_QUALITY_OK,
  SIGNAL_QUALITY_STALE,
  SIGNAL_QUALITY_ERROR,
} SignalQuality;

typedef struct {
  char key[SIGNAL_CACHE_KEY_MAX];
  char message_name[SIGNAL_CACHE_MESSAGE_MAX];
  char signal_name[SIGNAL_CACHE_SIGNAL_MAX];
  char unit[SIGNAL_CACHE_UNIT_MAX];
  double physical_value;
  int64_t raw_value;
  uint32_t updated_ms;
  SignalQuality quality;
} SignalCacheEntry;

typedef struct {
  SignalCacheEntry entries[SIGNAL_CACHE_MAX_VALUES];
  size_t count;
} SignalCache;

void signal_cache_init(SignalCache *cache);
bool signal_cache_upsert(SignalCache *cache,
                         const char *message_name,
                         const char *signal_name,
                         const char *unit,
                         double physical_value,
                         int64_t raw_value,
                         uint32_t now_ms);
const SignalCacheEntry *signal_cache_find(const SignalCache *cache, const char *key);
size_t signal_cache_copy(const SignalCache *cache, SignalCacheEntry *out_entries, size_t out_capacity);
size_t signal_cache_export_rule_snapshots(const SignalCache *cache,
                                          SignalSnapshot *out_signals,
                                          size_t out_capacity);
void signal_cache_mark_stale(SignalCache *cache, uint32_t now_ms, uint32_t stale_after_ms);

#endif
