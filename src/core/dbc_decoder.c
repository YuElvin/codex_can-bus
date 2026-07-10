#include "dbc_decoder.h"

size_t dbc_decode_frame_to_signal_cache(const DbcDatabase *db,
                                        const CanFrame *frame,
                                        SignalCache *cache,
                                        uint32_t now_ms) {
  if (db == NULL || frame == NULL || cache == NULL) {
    return 0u;
  }

  const DbcMessage *message = dbc_find_message(db, frame->id);
  if (message == NULL) {
    return 0u;
  }

  size_t updated = 0u;
  const size_t end = message->signal_start + message->signal_count;
  for (size_t i = message->signal_start; i < end; ++i) {
    const DbcSignal *signal = &db->signals[i];
    int64_t raw_value = 0;
    double physical_value = 0.0;
    if (!signal_extract_raw(frame->data, frame->dlc, &signal->spec, &raw_value) ||
        !dbc_decode_signal_value(signal, frame, &physical_value)) {
      continue;
    }
    if (signal_cache_upsert(cache,
                            message->name,
                            signal->name,
                            signal->unit,
                            physical_value,
                            raw_value,
                            now_ms)) {
      ++updated;
    }
  }
  return updated;
}
