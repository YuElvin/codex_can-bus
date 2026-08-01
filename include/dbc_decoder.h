#ifndef DBC_DECODER_H
#define DBC_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "dbc_parser.h"
#include "dbc_selected_runtime.h"
#include "signal_cache.h"

size_t dbc_decode_frame_to_signal_cache(const DbcDatabase *db,
                                        const CanFrame *frame,
                                        SignalCache *cache,
                                        uint32_t now_ms);

/*
 * Decode only signals in the active selected runtime. A frame matches only
 * when normalized ID, IDE, FD requirement, and actual payload length match.
 */
size_t dbc_decode_frame_to_selected_values(
  DbcSelectedRuntimeSnapshot *snapshot,
  const CanFrame *frame,
  uint32_t now_ms);

#endif
