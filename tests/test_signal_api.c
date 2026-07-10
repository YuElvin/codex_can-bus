#include "signal_api.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_empty_cache(void) {
  char body[96];
  ASSERT_TRUE(signal_api_build_json(NULL, 0u, body, sizeof(body)) > 0u);
  ASSERT_TRUE(strcmp(body, "{\"ok\":true,\"data\":{\"items\":[],\"count\":0}}") == 0);
  return 0;
}

static int test_two_item_limit_and_fields(void) {
  const SignalCacheEntry entries[] = {
    {.key = "Can2Data.marker", .unit = "count", .physical_value = 42434.0, .raw_value = 42434,
     .updated_ms = 42u, .quality = SIGNAL_QUALITY_OK},
    {.key = "Can2Data.sequence", .unit = "count", .physical_value = -12.5, .raw_value = -125,
     .updated_ms = 43u, .quality = SIGNAL_QUALITY_STALE},
    {.key = "ignored", .unit = "", .physical_value = 1.0, .raw_value = 1,
     .updated_ms = 44u, .quality = SIGNAL_QUALITY_ERROR},
  };
  char body[512];
  ASSERT_TRUE(signal_api_build_json(entries, 3u, body, sizeof(body)) > 0u);
  ASSERT_TRUE(strcmp(body,
                     "{\"ok\":true,\"data\":{\"items\":[{\"key\":\"Can2Data.marker\","
                     "\"value\":42434.000000,\"raw\":42434,\"unit\":\"count\","
                     "\"updated_ms\":42,\"quality\":\"ok\"},{\"key\":\"Can2Data.sequence\","
                     "\"value\":-12.500000,\"raw\":-125,\"unit\":\"count\","
                     "\"updated_ms\":43,\"quality\":\"stale\"}],\"count\":2}}") == 0);
  ASSERT_TRUE(strstr(body, "ignored") == NULL);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_empty_cache() == 0);
  ASSERT_TRUE(test_two_item_limit_and_fields() == 0);
  return 0;
}
