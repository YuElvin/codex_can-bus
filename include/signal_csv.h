#ifndef SIGNAL_CSV_H
#define SIGNAL_CSV_H

#include <stdbool.h>
#include <stddef.h>

#include "signal_cache.h"

#define SIGNAL_CSV_MAX_ITEMS 2u

size_t signal_csv_build_rows(const SignalCacheEntry *entries,
                             size_t entry_count,
                             bool include_header,
                             char *output,
                             size_t output_len);

size_t signal_csv_build_rows_v2(const SignalCacheEntry *entries,
                                size_t entry_count,
                                uint64_t unix_ms,
                                bool include_header,
                                char *output,
                                size_t output_len);

#endif
