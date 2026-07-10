#include "dbc_decoder.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);              \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

int main(void) {
  const char dbc_text[] =
    "BO_ 801 Can2Data: 8 Vector__XXX\n"
    " SG_ marker : 0|16@1+ (1,0) [0|65535] \"count\" Vector__XXX\n"
    " SG_ sequence : 16|16@1+ (1,0) [0|65535] \"count\" Vector__XXX\n";
  DbcDatabase db;
  SignalCache cache;
  size_t lines = 0u;
  ASSERT_TRUE(dbc_parse_text(&db, dbc_text, strlen(dbc_text), &lines));
  signal_cache_init(&cache);

  const CanFrame frame = {
    .id = 801u,
    .ide = CAN_ID_STANDARD,
    .dlc = 8u,
    .data = {0xc2u, 0xa5u, 0x34u, 0x12u},
  };
  ASSERT_TRUE(dbc_decode_frame_to_signal_cache(&db, &frame, &cache, 42u) == 2u);

  const SignalCacheEntry *marker = signal_cache_find(&cache, "Can2Data.marker");
  const SignalCacheEntry *sequence = signal_cache_find(&cache, "Can2Data.sequence");
  ASSERT_TRUE(marker != NULL && marker->raw_value == 0xa5c2 && marker->physical_value == 42434.0);
  ASSERT_TRUE(sequence != NULL && sequence->raw_value == 0x1234 && sequence->updated_ms == 42u);

  CanFrame unmatched = frame;
  unmatched.id = 0x123u;
  ASSERT_TRUE(dbc_decode_frame_to_signal_cache(&db, &unmatched, &cache, 43u) == 0u);
  ASSERT_TRUE(cache.count == 2u);
  return 0;
}
