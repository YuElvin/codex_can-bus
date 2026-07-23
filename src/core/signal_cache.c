#include "signal_cache.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char *dest, size_t dest_len, const char *src) {
  if (dest_len == 0u) {
    return;
  }
  snprintf(dest, dest_len, "%s", src != NULL ? src : "");
}

static bool make_key(char *dest, size_t dest_len, const char *message_name, const char *signal_name) {
  if (dest == NULL || dest_len == 0u || message_name == NULL || signal_name == NULL) {
    return false;
  }
  if (message_name[0] == '\0' || signal_name[0] == '\0') {
    return false;
  }
  return snprintf(dest, dest_len, "%s.%s", message_name, signal_name) < (int)dest_len;
}

static SignalCacheEntry *find_mutable(SignalCache *cache, const char *key) {
  if (cache == NULL || key == NULL) {
    return NULL;
  }

  for (size_t i = 0u; i < cache->count; ++i) {
    if (strcmp(cache->entries[i].key, key) == 0) {
      return &cache->entries[i];
    }
  }
  return NULL;
}

void signal_cache_init(SignalCache *cache) {
  if (cache != NULL) {
    memset(cache, 0, sizeof(*cache));
  }
}

bool signal_cache_upsert(SignalCache *cache,
                         const char *message_name,
                         const char *signal_name,
                         const char *unit,
                         double physical_value,
                         int64_t raw_value,
                         uint32_t now_ms) {
  if (cache == NULL) {
    return false;
  }

  char key[SIGNAL_CACHE_KEY_MAX] = {0};
  if (!make_key(key, sizeof(key), message_name, signal_name)) {
    return false;
  }

  SignalCacheEntry *entry = find_mutable(cache, key);
  if (entry == NULL) {
    if (cache->count >= SIGNAL_CACHE_MAX_VALUES) {
      return false;
    }
    entry = &cache->entries[cache->count];
    ++cache->count;
  }

  copy_text(entry->key, sizeof(entry->key), key);
  copy_text(entry->message_name, sizeof(entry->message_name), message_name);
  copy_text(entry->signal_name, sizeof(entry->signal_name), signal_name);
  copy_text(entry->unit, sizeof(entry->unit), unit);
  entry->physical_value = physical_value;
  entry->raw_value = raw_value;
  entry->updated_ms = now_ms;
  entry->quality = SIGNAL_QUALITY_OK;
  return true;
}

const SignalCacheEntry *signal_cache_find(const SignalCache *cache, const char *key) {
  if (cache == NULL || key == NULL) {
    return NULL;
  }

  for (size_t i = 0u; i < cache->count; ++i) {
    if (strcmp(cache->entries[i].key, key) == 0) {
      return &cache->entries[i];
    }
  }
  return NULL;
}

size_t signal_cache_copy(const SignalCache *cache, SignalCacheEntry *out_entries, size_t out_capacity) {
  if (cache == NULL || out_entries == NULL || out_capacity == 0u) {
    return 0u;
  }

  const size_t count = cache->count < out_capacity ? cache->count : out_capacity;
  memcpy(out_entries, cache->entries, count * sizeof(out_entries[0]));
  return count;
}

size_t signal_cache_export_rule_snapshots(const SignalCache *cache,
                                          SignalSnapshot *out_signals,
                                          size_t out_capacity) {
  if (cache == NULL || out_signals == NULL || out_capacity == 0u) {
    return 0u;
  }

  size_t written = 0u;
  for (size_t i = 0u; i < cache->count && written < out_capacity; ++i) {
    const SignalCacheEntry *entry = &cache->entries[i];
    copy_text(out_signals[written].key, sizeof(out_signals[written].key), entry->key);
    out_signals[written].value = entry->physical_value;
    out_signals[written].updated_ms = entry->updated_ms;
    out_signals[written].valid = entry->quality == SIGNAL_QUALITY_OK;
    ++written;
  }
  return written;
}

size_t signal_cache_export_rule_snapshots_for_engine(const SignalCache *cache,
                                                     const RuleEngine *engine,
                                                     SignalSnapshot *out_signals,
                                                     size_t out_capacity) {
  size_t written = 0u;

  if (cache == NULL || engine == NULL || out_signals == NULL || out_capacity == 0u) {
    return 0u;
  }
  for (size_t rule_index = 0u; rule_index < engine->rule_count && written < out_capacity;
       ++rule_index) {
    const Rule *rule = &engine->rules[rule_index];
    bool duplicate = false;
    if (!rule->enabled) {
      continue;
    }
    for (size_t i = 0u; i < written; ++i) {
      if (strcmp(out_signals[i].key, rule->signal_key) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }
    const SignalCacheEntry *entry = signal_cache_find(cache, rule->signal_key);
    if (entry == NULL) {
      continue;
    }
    copy_text(out_signals[written].key, sizeof(out_signals[written].key), entry->key);
    out_signals[written].value = entry->physical_value;
    out_signals[written].updated_ms = entry->updated_ms;
    out_signals[written].valid = entry->quality == SIGNAL_QUALITY_OK;
    ++written;
  }
  return written;
}

void signal_cache_mark_stale(SignalCache *cache, uint32_t now_ms, uint32_t stale_after_ms) {
  if (cache == NULL || stale_after_ms == 0u) {
    return;
  }

  for (size_t i = 0u; i < cache->count; ++i) {
    SignalCacheEntry *entry = &cache->entries[i];
    if (entry->quality == SIGNAL_QUALITY_OK && now_ms - entry->updated_ms > stale_after_ms) {
      entry->quality = SIGNAL_QUALITY_STALE;
    }
  }
}
