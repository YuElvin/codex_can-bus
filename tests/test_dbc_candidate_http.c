#include "dbc_candidate_http.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition) do { if (!(condition)) { \
  fprintf(stderr, "ASSERT failed at %s:%d: %s\n", __FILE__, __LINE__, \
          #condition); \
  return false; \
} } while (0)

static bool parse_get(const char *target, DbcCandidateCatalogQuery *query,
                      char storage[DBC_CANDIDATE_HTTP_QUERY_STORAGE_BYTES],
                      DbcCandidateHttpStatus expected) {
  ASSERT_TRUE(dbc_candidate_http_parse_get_target(
                target, strlen(target), query, storage) == expected);
  return true;
}

static bool test_get_target_success(void) {
  DbcCandidateCatalogQuery query;
  char storage[DBC_CANDIDATE_HTTP_QUERY_STORAGE_BYTES];
  ASSERT_TRUE(parse_get(
    "/api/dbc/candidate/signals?page=4294967295&q=Volt%22age+X&selected=true&pageSize=8",
    &query, storage, DBC_CANDIDATE_HTTP_OK));
  ASSERT_TRUE(query.page == UINT32_MAX && query.page_size == 8u &&
              query.selected_filter == DBC_CANDIDATE_FILTER_SELECTED &&
              query.query == storage && query.query_length == 10u &&
              strcmp(storage, "Volt\"age X") == 0);

  ASSERT_TRUE(parse_get("?selected=false", &query, storage,
                        DBC_CANDIDATE_HTTP_OK));
  ASSERT_TRUE(query.page == 0u &&
              query.selected_filter == DBC_CANDIDATE_FILTER_UNSELECTED &&
              query.query_length == 0u);
  ASSERT_TRUE(parse_get("page=7&q=&selected=all", &query, storage,
                        DBC_CANDIDATE_HTTP_OK));
  ASSERT_TRUE(query.page == 7u && query.selected_filter == DBC_CANDIDATE_FILTER_ALL);
  ASSERT_TRUE(parse_get("/api/dbc/candidate/signals", &query, storage,
                        DBC_CANDIDATE_HTTP_OK));
  ASSERT_TRUE(query.page == 0u && query.page_size == 8u);
  ASSERT_TRUE(parse_get("q=%E6%B8%A9%E5%BA%A6", &query, storage,
                        DBC_CANDIDATE_HTTP_OK));
  static const char expected_utf8[] = "\xE6\xB8\xA9\xE5\xBA\xA6";
  ASSERT_TRUE(query.query_length == sizeof(expected_utf8) - 1u &&
              memcmp(storage, expected_utf8, sizeof(expected_utf8)) == 0);
  return true;
}

static bool test_get_target_rejections(void) {
  DbcCandidateCatalogQuery query;
  char storage[DBC_CANDIDATE_HTTP_QUERY_STORAGE_BYTES];
  ASSERT_TRUE(parse_get("/wrong?page=0", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_TARGET));
  ASSERT_TRUE(parse_get("page=4294967296", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_NUMBER));
  ASSERT_TRUE(parse_get("page=-1", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_NUMBER));
  ASSERT_TRUE(parse_get("pageSize=7", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_NUMBER));
  ASSERT_TRUE(parse_get("selected=yes", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_TARGET));
  ASSERT_TRUE(parse_get("q=bad%2", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_ENCODING));
  ASSERT_TRUE(parse_get("q=bad%GG", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_ENCODING));
  ASSERT_TRUE(parse_get("q=bad%00", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_ENCODING));
  ASSERT_TRUE(parse_get("page=1&page=2", &query, storage,
                        DBC_CANDIDATE_HTTP_DUPLICATE_FIELD));
  ASSERT_TRUE(parse_get("unknown=1", &query, storage,
                        DBC_CANDIDATE_HTTP_UNKNOWN_FIELD));
  ASSERT_TRUE(parse_get("q=ok&&page=1", &query, storage,
                        DBC_CANDIDATE_HTTP_INVALID_TARGET));

  char too_long[80];
  memcpy(too_long, "q=", 2u);
  memset(too_long + 2u, 'a', 64u);
  too_long[66] = '\0';
  ASSERT_TRUE(parse_get(too_long, &query, storage,
                        DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED));
  return true;
}

static DbcCandidateHttpStatus parse_form(
  const char *body, DbcCandidateHttpSelectionForm *form) {
  return dbc_candidate_http_parse_selection_form(body, strlen(body), form);
}

static bool test_selection_form_success(void) {
  DbcCandidateHttpSelectionForm form;
  const char *token = "0000000000000011-000186E7-4B88D9CE";
  char body[160];
  const int length = snprintf(
    body, sizeof(body), "candidateToken=%s&set=1%%2C2%%2C65535&clear=3", token);
  ASSERT_TRUE(length > 0 && (size_t)length < sizeof(body));
  ASSERT_TRUE(parse_form(
    body, &form) == DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(strcmp(form.candidate_token, token) == 0 &&
              form.set_count == 3u && form.clear_count == 1u &&
              form.set_ordinals[0] == 1u && form.set_ordinals[2] == UINT16_MAX &&
              form.clear_ordinals[0] == 3u);
  const int empty_length = snprintf(
    body, sizeof(body), "candidateToken=%s&set=&clear=7", token);
  ASSERT_TRUE(empty_length > 0 && (size_t)empty_length < sizeof(body));
  ASSERT_TRUE(parse_form(body, &form) ==
              DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(form.set_count == 0u && form.clear_count == 1u);
  return true;
}

static bool test_selection_form_rejections(void) {
  DbcCandidateHttpSelectionForm form;
  const char *prefix =
    "candidateToken=0000000000000011-000186E7-4B88D9CE&";
  char body[256];
#define ASSERT_FORM_SUFFIX(suffix, expected) do { \
  const int length = snprintf(body, sizeof(body), "%s%s", prefix, suffix); \
  ASSERT_TRUE(length > 0 && (size_t)length < sizeof(body)); \
  ASSERT_TRUE(parse_form(body, &form) == (expected)); \
} while (0)
  ASSERT_FORM_SUFFIX("set=&clear=",
              DBC_CANDIDATE_HTTP_INVALID_ARGUMENT);
  ASSERT_FORM_SUFFIX("set=65536",
              DBC_CANDIDATE_HTTP_INVALID_NUMBER);
  ASSERT_FORM_SUFFIX("set=1,1",
              DBC_CANDIDATE_HTTP_MUTATION_CONFLICT);
  ASSERT_FORM_SUFFIX("set=1&clear=1",
              DBC_CANDIDATE_HTTP_MUTATION_CONFLICT);
  ASSERT_FORM_SUFFIX("set=1&set=2",
              DBC_CANDIDATE_HTTP_DUPLICATE_FIELD);
  ASSERT_FORM_SUFFIX("set=1&wat=2",
              DBC_CANDIDATE_HTTP_UNKNOWN_FIELD);
  ASSERT_FORM_SUFFIX("set=1,",
              DBC_CANDIDATE_HTTP_INVALID_NUMBER);
  ASSERT_FORM_SUFFIX("set=%00",
              DBC_CANDIDATE_HTTP_INVALID_ENCODING);
  ASSERT_TRUE(parse_form("set=1", &form) ==
              DBC_CANDIDATE_HTTP_INVALID_TARGET);
  ASSERT_TRUE(parse_form("candidateToken=bad&set=1", &form) ==
              DBC_CANDIDATE_HTTP_INVALID_TOKEN);
  ASSERT_TRUE(parse_form(
    "candidateToken=0000000000000011-000186E7-4B88D9CE&candidateToken=0000000000000011-000186E7-4B88D9CE&set=1",
    &form) == DBC_CANDIDATE_HTTP_DUPLICATE_FIELD);

  char long_token[80];
  const int token_length = snprintf(long_token, sizeof(long_token),
                                    "candidateToken=%035u&set=1", 1u);
  ASSERT_TRUE(token_length > 0 && (size_t)token_length < sizeof(long_token));
  ASSERT_TRUE(parse_form(long_token, &form) ==
              DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED);

  char many[256] =
    "candidateToken=0000000000000011-000186E7-4B88D9CE&set=";
  size_t length = strlen(many);
  for (unsigned i = 0u; i < 33u; ++i) {
    const int added = snprintf(many + length, sizeof(many) - length,
                               "%s%u", i == 0u ? "" : ",", i);
    ASSERT_TRUE(added > 0 && (size_t)added < sizeof(many) - length);
    length += (size_t)added;
  }
  ASSERT_TRUE(parse_form(many, &form) == DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED);

  char oversized[LARGE_DBC_SELECTION_REQUEST_BODY_BYTES + 2u];
  memset(oversized, 'x', sizeof(oversized));
  ASSERT_TRUE(dbc_candidate_http_parse_selection_form(
                oversized, LARGE_DBC_SELECTION_REQUEST_BODY_BYTES + 1u,
                &form) == DBC_CANDIDATE_HTTP_LIMIT_EXCEEDED);
#undef ASSERT_FORM_SUFFIX
  return true;
}

static bool make_descriptor(DbcCandidateDescriptor *descriptor,
                            char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES]) {
  *descriptor = (DbcCandidateDescriptor){
    .generation = 17u,
    .selection_generation = 17u,
    .source_size = 100071u,
    .source_crc32 = UINT32_C(0x4B88D9CE),
    .catalog_message_count = 112u,
    .catalog_signal_count = 896u,
    .selected_count = 6u,
    .selected_message_count = 3u
  };
  ASSERT_TRUE(dbc_candidate_token_format(descriptor->generation,
                                          descriptor->source_size,
                                          descriptor->source_crc32, token) ==
              DBC_CANDIDATE_FORMAT_OK);
  return true;
}

static void make_page(DbcCandidateCatalogPage *page, uint8_t count) {
  memset(page, 0, sizeof(*page));
  page->catalog_total = 896u;
  page->matched_total = count;
  page->page = 0u;
  page->page_size = 8u;
  page->item_count = count;
  for (uint8_t i = 0u; i < count; ++i) {
    page->items[i].ordinal = i;
    page->items[i].selected = (i & 1u) != 0u;
  }
}

static bool test_catalog_json_escape_and_capacity(void) {
  DbcCandidateDescriptor descriptor;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  ASSERT_TRUE(make_descriptor(&descriptor, token));
  DbcCandidateCatalogPage page;
  make_page(&page, 2u);
  memcpy(page.items[0].key, "quote\"slash\\line\n", 18u);
  page.items[0].key[18] = '\0';
  const char utf8_key[] = "Temp.\xE6\xB8\xA9\xE5\xBA\xA6";
  memcpy(page.items[1].key, utf8_key, sizeof(utf8_key));

  char output[LARGE_DBC_HTTP_JSON_BODY_BYTES];
  size_t written = 0u;
  ASSERT_TRUE(dbc_candidate_http_serialize_catalog_json(
                token, &descriptor, &page, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(written == strlen(output));
  ASSERT_TRUE(strstr(output, "\\\"slash\\\\line\\n") != NULL);
  ASSERT_TRUE(strstr(output, utf8_key) != NULL);
  ASSERT_TRUE(strstr(output, "\"pageSize\":8") != NULL &&
              strstr(output, "\"selectedCount\":6") != NULL);

  const size_t required = written;
  ASSERT_TRUE(dbc_candidate_http_serialize_catalog_json(
                token, &descriptor, &page, output, required, &written) ==
              DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED);
  ASSERT_TRUE(written == 0u && output[0] == '\0');
  ASSERT_TRUE(dbc_candidate_http_serialize_catalog_json(
                token, &descriptor, &page, output, required + 1u, &written) ==
              DBC_CANDIDATE_HTTP_OK && written == required);
  return true;
}

static bool test_catalog_initial_zero_selection(void) {
  DbcCandidateDescriptor descriptor;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  ASSERT_TRUE(make_descriptor(&descriptor, token));
  descriptor.selected_count = 0u;
  descriptor.selected_message_count = 0u;
  DbcCandidateCatalogPage page;
  make_page(&page, 0u);
  char output[LARGE_DBC_HTTP_JSON_BODY_BYTES];
  size_t written = 0u;
  ASSERT_TRUE(dbc_candidate_http_serialize_catalog_json(
                token, &descriptor, &page, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(written == strlen(output) &&
              strstr(output, "\"selectedCount\":0") != NULL);
  return true;
}

static bool test_catalog_worst_eight_and_payload_limit(void) {
  DbcCandidateDescriptor descriptor;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  ASSERT_TRUE(make_descriptor(&descriptor, token));
  DbcCandidateCatalogPage page;
  make_page(&page, 8u);
  for (uint8_t item = 0u; item < 8u; ++item) {
    for (size_t i = 0u; i < LARGE_DBC_KEY_MAX_BYTES; ++i) {
      static const char pattern[] = {'\"', '\\', '\n', 'A'};
      page.items[item].key[i] = pattern[i % sizeof(pattern)];
    }
    page.items[item].key[LARGE_DBC_KEY_MAX_BYTES] = '\0';
  }
  char output[LARGE_DBC_HTTP_JSON_BODY_BYTES];
  size_t written = 0u;
  ASSERT_TRUE(dbc_candidate_http_serialize_catalog_json(
                token, &descriptor, &page, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(written <= LARGE_DBC_HTTP_JSON_PAYLOAD_MAX_BYTES &&
              written == strlen(output));

  for (uint8_t item = 0u; item < 8u; ++item) {
    memset(page.items[item].key, 1, LARGE_DBC_KEY_MAX_BYTES);
    page.items[item].key[LARGE_DBC_KEY_MAX_BYTES] = '\0';
  }
  ASSERT_TRUE(dbc_candidate_http_serialize_catalog_json(
                token, &descriptor, &page, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_CAPACITY_EXCEEDED);
  ASSERT_TRUE(written == 0u && output[0] == '\0');
  return true;
}

static bool test_selection_json(void) {
  DbcCandidateDescriptor descriptor;
  char token[LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES];
  ASSERT_TRUE(make_descriptor(&descriptor, token));
  char output[256];
  size_t written = 0u;
  ASSERT_TRUE(dbc_candidate_http_serialize_selection_json(
                token, &descriptor, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(written == strlen(output) &&
              strstr(output, "\"generation\":17") != NULL &&
              strstr(output, "\"selectedCount\":6") != NULL &&
              strstr(output, "\"selectedMessageCount\":3") != NULL);
  token[0] = token[0] == '0' ? '1' : '0';
  ASSERT_TRUE(dbc_candidate_http_serialize_selection_json(
                token, &descriptor, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_INVALID_STATE);
  ASSERT_TRUE(written == 0u);

  descriptor.generation = UINT64_MAX;
  descriptor.selection_generation = UINT64_MAX;
  memcpy(token, "FFFFFFFFFFFFFFFF-000186E7-4B88D9CE",
         LARGE_DBC_CANDIDATE_TOKEN_BUFFER_BYTES);
  ASSERT_TRUE(dbc_candidate_http_serialize_selection_json(
                token, &descriptor, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(strstr(output, "\"generation\":18446744073709551615") != NULL);

  descriptor.selected_count = 0u;
  descriptor.selected_message_count = 0u;
  ASSERT_TRUE(dbc_candidate_http_serialize_selection_json(
                token, &descriptor, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_OK);
  ASSERT_TRUE(written == strlen(output) &&
              strstr(output, "\"selectedCount\":0") != NULL &&
              strstr(output, "\"selectedMessageCount\":0") != NULL);
  descriptor.selected_message_count = 1u;
  ASSERT_TRUE(dbc_candidate_http_serialize_selection_json(
                token, &descriptor, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_INVALID_STATE);
  descriptor.selected_count = 1u;
  descriptor.selected_message_count = 0u;
  ASSERT_TRUE(dbc_candidate_http_serialize_selection_json(
                token, &descriptor, output, sizeof(output), &written) ==
              DBC_CANDIDATE_HTTP_INVALID_STATE);
  return true;
}

int main(void) {
  if (!test_get_target_success() || !test_get_target_rejections() ||
      !test_selection_form_success() ||
      !test_selection_form_rejections() ||
      !test_catalog_json_escape_and_capacity() ||
      !test_catalog_initial_zero_selection() ||
      !test_catalog_worst_eight_and_payload_limit() ||
      !test_selection_json()) {
    return 1;
  }
  puts("dbc candidate http tests passed");
  return 0;
}
