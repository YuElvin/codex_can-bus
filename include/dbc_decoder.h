#ifndef DBC_DECODER_H
#define DBC_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "dbc_parser.h"
#include "signal_cache.h"

size_t dbc_decode_frame_to_signal_cache(const DbcDatabase *db,
                                        const CanFrame *frame,
                                        SignalCache *cache,
                                        uint32_t now_ms);

#endif
