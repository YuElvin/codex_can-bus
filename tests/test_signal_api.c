#include "signal_api.h"

#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_STATUS(expr, expected) ASSERT_TRUE((expr) == (expected))

static DbcSelectedRuntimeSnapshot g_runtime;

static void prepare_runtime(uint16_t signal_count) {
  dbc_selected_runtime_snapshot_init(&g_runtime);
  g_runtime.has_active = true;
  g_runtime.active_slot = 0u;
  DbcSelectedRuntime *runtime = &g_runtime.slots[0];
  runtime->runtime_generation = UINT64_C(0x123456789ABCDEF0);
  runtime->selection_crc32 = UINT32_C(0x62BA0D66);
  runtime->signal_count = signal_count;
  for (uint16_t i = 0u; i < signal_count; ++i) {
    DbcSelectedRuntimeSignal *signal = &runtime->signals[i];
    signal->catalog_ordinal = (uint16_t)(100u + i);
    signal->value_state_index = i;
    snprintf(signal->key, sizeof(signal->key), "Message%03u.Signal%03u",
             (unsigned)i, (unsigned)i);
    snprintf(signal->unit, sizeof(signal->unit), "unit%u", (unsigned)i);
    g_runtime.value_slots[0][i].quality = SIGNAL_VALUE_QUALITY_MISSING;
  }
}

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

static int test_query_parser(void) {
  SignalApiQuery query;
  const char plain[] = "/api/signals";
  ASSERT_STATUS(signal_api_parse_get_target(plain, strlen(plain), &query),
                SIGNAL_API_OK);
  ASSERT_TRUE(query.page == 0u && query.query[0] == '\0');

  const char complete[] = "/api/signals?q=Pack%20Voltage&page=15";
  ASSERT_STATUS(signal_api_parse_get_target(complete, strlen(complete), &query),
                SIGNAL_API_OK);
  ASSERT_TRUE(query.page == 15u && strcmp(query.query, "Pack Voltage") == 0);

  const char max_page[] = "/api/signals?page=4294967295&q=";
  ASSERT_STATUS(signal_api_parse_get_target(max_page, strlen(max_page), &query),
                SIGNAL_API_OK);
  ASSERT_TRUE(query.page == UINT32_MAX && query.query[0] == '\0');

  const char overflow[] = "/api/signals?page=4294967296";
  ASSERT_STATUS(signal_api_parse_get_target(overflow, strlen(overflow), &query),
                SIGNAL_API_INVALID_PAGE);
  const char duplicate[] = "/api/signals?page=0&page=1";
  ASSERT_STATUS(signal_api_parse_get_target(duplicate, strlen(duplicate), &query),
                SIGNAL_API_DUPLICATE_FIELD);
  const char unknown[] = "/api/signals?page=0&selected=true";
  ASSERT_STATUS(signal_api_parse_get_target(unknown, strlen(unknown), &query),
                SIGNAL_API_UNKNOWN_FIELD);
  const char invalid_percent[] = "/api/signals?q=%0";
  ASSERT_STATUS(signal_api_parse_get_target(invalid_percent,
                                             strlen(invalid_percent), &query),
                SIGNAL_API_INVALID_QUERY);
  const char control[] = "/api/signals?q=%01";
  ASSERT_STATUS(signal_api_parse_get_target(control, strlen(control), &query),
                SIGNAL_API_INVALID_QUERY);
  const char trailing[] = "/api/signals?page=0&";
  ASSERT_STATUS(signal_api_parse_get_target(trailing, strlen(trailing), &query),
                SIGNAL_API_INVALID_TARGET);
  const char wrong[] = "/api/signals-extra?page=0";
  ASSERT_STATUS(signal_api_parse_get_target(wrong, strlen(wrong), &query),
                SIGNAL_API_INVALID_TARGET);

  char max_query_target[96] = "/api/signals?q=";
  memset(max_query_target + strlen(max_query_target), 'x',
         LARGE_DBC_QUERY_MAX_BYTES);
  const size_t max_length = strlen("/api/signals?q=") +
                            LARGE_DBC_QUERY_MAX_BYTES;
  max_query_target[max_length] = '\0';
  ASSERT_STATUS(signal_api_parse_get_target(max_query_target, max_length, &query),
                SIGNAL_API_OK);
  max_query_target[max_length] = 'x';
  max_query_target[max_length + 1u] = '\0';
  ASSERT_STATUS(signal_api_parse_get_target(max_query_target, max_length + 1u,
                                             &query),
                SIGNAL_API_INVALID_QUERY);
  return 0;
}

static int test_stable_pages_and_filter(void) {
  static const uint16_t counts[] = {1u, 16u, 64u, 128u};
  for (size_t count_index = 0u;
       count_index < sizeof(counts) / sizeof(counts[0]); ++count_index) {
    const uint16_t count = counts[count_index];
    prepare_runtime(count);
    SignalApiQuery query = {.page = (uint32_t)((count - 1u) / 8u)};
    SignalApiPage page;
    ASSERT_STATUS(signal_api_build_selected_page(
                    &g_runtime, &query, 100u,
                    LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page),
                  SIGNAL_API_OK);
    ASSERT_TRUE(page.total == count && page.matched == count &&
                page.page_size == 8u && page.item_count >= 1u &&
                page.item_count <= 8u);
    const uint16_t first = (uint16_t)(query.page * 8u);
    ASSERT_TRUE(page.items[0].signal->catalog_ordinal ==
                (uint16_t)(100u + first));
  }

  prepare_runtime(128u);
  SignalApiQuery query = {.page = 0u};
  memcpy(query.query, "mEsSaGe12", sizeof("mEsSaGe12"));
  SignalApiPage page;
  ASSERT_STATUS(signal_api_build_selected_page(
                  &g_runtime, &query, 100u,
                  LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page),
                SIGNAL_API_OK);
  ASSERT_TRUE(page.total == 128u && page.matched == 8u &&
              page.item_count == 8u &&
              page.items[0].signal->catalog_ordinal == 220u &&
              page.items[7].signal->catalog_ordinal == 227u);

  query.page = 1u;
  ASSERT_STATUS(signal_api_build_selected_page(
                  &g_runtime, &query, 100u,
                  LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page),
                SIGNAL_API_OK);
  ASSERT_TRUE(page.matched == 8u && page.item_count == 0u);
  return 0;
}

static int test_quality_semantics_and_json(void) {
  prepare_runtime(4u);
  SignalValueState *values = g_runtime.value_slots[0];
  values[0].quality = SIGNAL_VALUE_QUALITY_GOOD;
  values[0].update_seq = 0u;
  values[1].value = 12.5;
  values[1].raw = 125;
  values[1].updated_ms = 1000u;
  values[1].quality = SIGNAL_VALUE_QUALITY_GOOD;
  values[1].update_seq = 2u;
  values[2].value = -12.5;
  values[2].raw = -125;
  values[2].updated_ms = 999u;
  values[2].quality = SIGNAL_VALUE_QUALITY_GOOD;
  values[2].update_seq = 2u;
  values[3].value = 99.0;
  values[3].raw = 99;
  values[3].updated_ms = 900u;
  values[3].quality = SIGNAL_VALUE_QUALITY_ERROR;
  values[3].update_seq = 2u;

  SignalApiQuery query = {0};
  SignalApiPage page;
  ASSERT_STATUS(signal_api_build_selected_page(
                  &g_runtime, &query, 4000u,
                  LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page),
                SIGNAL_API_OK);
  ASSERT_TRUE(page.items[0].value.quality == SIGNAL_VALUE_QUALITY_MISSING &&
              page.items[0].value.updated_ms == 0u);
  ASSERT_TRUE(page.items[1].value.quality == SIGNAL_VALUE_QUALITY_GOOD);
  ASSERT_TRUE(page.items[2].value.quality == SIGNAL_VALUE_QUALITY_STALE);
  ASSERT_TRUE(page.items[3].value.quality == SIGNAL_VALUE_QUALITY_ERROR);

  char body[LARGE_DBC_HTTP_JSON_BODY_BYTES];
  size_t written = 0u;
  ASSERT_STATUS(signal_api_serialize_selected_page(&page, body, sizeof(body),
                                                    &written),
                SIGNAL_API_OK);
  ASSERT_TRUE(written == strlen(body));
  ASSERT_TRUE(strstr(body, "\"generation\":\"123456789ABCDEF0\"") != NULL);
  ASSERT_TRUE(strstr(body, "\"selectionCrc32\":\"62BA0D66\"") != NULL);
  ASSERT_TRUE(strstr(body, "\"ordinal\":100") != NULL);
  ASSERT_TRUE(strstr(body,
                     "\"value\":null,\"raw\":null,\"unit\":\"unit0\","
                     "\"quality\":\"MISSING\",\"updatedMs\":0") != NULL);
  ASSERT_TRUE(strstr(body, "\"quality\":\"GOOD\"") != NULL);
  ASSERT_TRUE(strstr(body, "\"quality\":\"STALE\"") != NULL);
  ASSERT_TRUE(strstr(body,
                     "\"value\":null,\"raw\":null,\"unit\":\"unit3\","
                     "\"quality\":\"ERROR\"") != NULL);

  values[1].updated_ms = UINT32_MAX - 9u;
  ASSERT_STATUS(signal_api_build_selected_page(
                  &g_runtime, &query, 10u,
                  LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page),
                SIGNAL_API_OK);
  ASSERT_TRUE(page.items[1].value.quality == SIGNAL_VALUE_QUALITY_GOOD);
  return 0;
}

static int test_json_escape_and_capacity(void) {
  prepare_runtime(1u);
  DbcSelectedRuntimeSignal *signal = &g_runtime.slots[0].signals[0];
  const char escaped_key[] = {'A', '\"', '\\', 0x01, (char)0xE4,
                              (char)0xB8, (char)0xAD, '\0'};
  const char escaped_unit[] = {'V', '\n', '\t', '\0'};
  memcpy(signal->key, escaped_key, sizeof(escaped_key));
  memcpy(signal->unit, escaped_unit, sizeof(escaped_unit));
  SignalValueState *value = &g_runtime.value_slots[0][0];
  value->value = 0.00000001;
  value->raw = INT64_MIN;
  value->updated_ms = UINT32_MAX;
  value->quality = SIGNAL_VALUE_QUALITY_GOOD;
  value->update_seq = 2u;
  SignalApiQuery query = {0};
  SignalApiPage page;
  ASSERT_STATUS(signal_api_build_selected_page(
                  &g_runtime, &query, UINT32_MAX,
                  LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page),
                SIGNAL_API_OK);
  char body[LARGE_DBC_HTTP_JSON_BODY_BYTES];
  size_t written = 0u;
  ASSERT_STATUS(signal_api_serialize_selected_page(&page, body, sizeof(body),
                                                    &written),
                SIGNAL_API_OK);
  ASSERT_TRUE(strstr(body, "A\\\"\\\\\\u0001") != NULL);
  ASSERT_TRUE(strstr(body, "\\n\\t") != NULL);
  ASSERT_TRUE(strstr(body, "-9223372036854775808") != NULL);
  ASSERT_TRUE(strstr(body, "E-8") != NULL);

  prepare_runtime(8u);
  for (uint8_t i = 0u; i < 8u; ++i) {
    signal = &g_runtime.slots[0].signals[i];
    memset(signal->key, '\"', LARGE_DBC_KEY_MAX_BYTES);
    signal->key[LARGE_DBC_KEY_MAX_BYTES] = '\0';
    memset(signal->unit, '\\', LARGE_DBC_UNIT_MAX_BYTES);
    signal->unit[LARGE_DBC_UNIT_MAX_BYTES] = '\0';
    value = &g_runtime.value_slots[0][i];
    value->value = i == 0u ? DBL_MAX : (double)(i + 1u);
    value->raw = INT64_MIN;
    value->updated_ms = UINT32_MAX;
    value->quality = SIGNAL_VALUE_QUALITY_GOOD;
    value->update_seq = 2u;
  }
  ASSERT_STATUS(signal_api_build_selected_page(
                  &g_runtime, &query, UINT32_MAX,
                  LARGE_DBC_SIGNAL_STALE_AFTER_MS, &page),
                SIGNAL_API_OK);
  ASSERT_STATUS(signal_api_serialize_selected_page(&page, body, sizeof(body),
                                                    &written),
                SIGNAL_API_OK);
  ASSERT_TRUE(written <= LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES &&
              written > LARGE_DBC_HTTP_RESPONSE_SEGMENT_BYTES);
  char exact[LARGE_DBC_HTTP_JSON_BODY_BYTES];
  size_t exact_written = 123u;
  exact[0] = 'x';
  ASSERT_STATUS(signal_api_serialize_selected_page(&page, exact, written,
                                                    &exact_written),
                SIGNAL_API_CAPACITY_EXCEEDED);
  ASSERT_TRUE(exact[0] == '\0' && exact_written == 0u);
  ASSERT_STATUS(signal_api_serialize_selected_page(&page, exact, written + 1u,
                                                    &exact_written),
                SIGNAL_API_OK);
  ASSERT_TRUE(exact_written == written && exact[written] == '\0');
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_empty_cache() == 0);
  ASSERT_TRUE(test_two_item_limit_and_fields() == 0);
  ASSERT_TRUE(test_query_parser() == 0);
  ASSERT_TRUE(test_stable_pages_and_filter() == 0);
  ASSERT_TRUE(test_quality_semantics_and_json() == 0);
  ASSERT_TRUE(test_json_escape_and_capacity() == 0);
  return 0;
}
