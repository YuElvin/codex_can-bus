#include "signal_csv.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_rows_include_header_and_escape_fields(void) {
  const SignalCacheEntry entries[] = {
    {.key = "Can2Data.marker", .unit = "count", .physical_value = 42434.0, .raw_value = 42434,
     .updated_ms = 42u, .quality = SIGNAL_QUALITY_OK},
    {.key = "quoted,signal", .unit = "\"V\"", .physical_value = -12.5, .raw_value = -125,
     .updated_ms = 43u, .quality = SIGNAL_QUALITY_STALE},
    {.key = "ignored", .unit = "", .physical_value = 1.0, .raw_value = 1,
     .updated_ms = 44u, .quality = SIGNAL_QUALITY_ERROR},
  };
  char body[512];

  ASSERT_TRUE(signal_csv_build_rows(entries, 3u, true, body, sizeof(body)) > 0u);
  ASSERT_TRUE(strcmp(body,
                     "updated_ms,key,value,raw,unit,quality\n"
                     "42,\"Can2Data.marker\",42434.000000,42434,\"count\",\"ok\"\n"
                     "43,\"quoted,signal\",-12.500000,-125,\"\"\"V\"\"\",\"stale\"\n") == 0);
  ASSERT_TRUE(strstr(body, "ignored") == NULL);
  return 0;
}

static int test_empty_snapshot_only_writes_header(void) {
  char body[64];
  ASSERT_TRUE(signal_csv_build_rows(NULL, 0u, true, body, sizeof(body)) > 0u);
  ASSERT_TRUE(strcmp(body, "updated_ms,key,value,raw,unit,quality\n") == 0);
  return 0;
}

static int test_v2_rows_include_utc_time(void) {
  const SignalCacheEntry entry = {.key = "Can2Data.marker", .unit = "count", .physical_value = 42.0,
                                  .raw_value = 42, .updated_ms = 7u, .quality = SIGNAL_QUALITY_OK};
  char body[256];

  ASSERT_TRUE(signal_csv_build_rows_v2(&entry, 1u, 0u, true, body, sizeof(body)) > 0u);
  ASSERT_TRUE(strcmp(body,
                     "utc_time,unix_ms,updated_ms,key,value,raw,unit,quality\n"
                     "1970-01-01T00:00:00.000Z,0,7,\"Can2Data.marker\",42.000000,42,\"count\",\"ok\"\n") == 0);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_rows_include_header_and_escape_fields() == 0);
  ASSERT_TRUE(test_empty_snapshot_only_writes_header() == 0);
  ASSERT_TRUE(test_v2_rows_include_utc_time() == 0);
  return 0;
}
