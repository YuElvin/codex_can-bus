#ifndef SIGNAL_API_H
#define SIGNAL_API_H

#include <stddef.h>

#include "signal_cache.h"

#define SIGNAL_API_MAX_ITEMS 2u

size_t signal_api_build_json(const SignalCacheEntry *entries,
                             size_t entry_count,
                             char *body,
                             size_t body_len);

#endif
