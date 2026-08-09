#include "selected_signal_log.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expression)                                                   \
  do {                                                                            \
    if (!(expression)) {                                                          \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__,           \
             #expression);                                                        \
      return false;                                                               \
    }                                                                             \
  } while (0)

static SelectedSignalLogIdentity make_identity(void) {
  SelectedSignalLogIdentity identity = {
    .meta_format_version = SELECTED_SIGNAL_LOG_META_FORMAT_VERSION,
    .csv_format_version = SELECTED_SIGNAL_LOG_CSV_FORMAT_VERSION,
    .selected_count = 16u,
    .active_generation = 7u,
    .candidate_generation = 9u,
    .source_size = 100071u,
    .source_crc32 = UINT32_C(0x4B88D9CE),
    .selection_crc32 = UINT32_C(0x62BA0D66),
    .sample_period_ms = 1000u,
    .start_unix_ms = UINT64_C(1709251199123)
  };
  strcpy(identity.firmware_build_id, "test-build-0123456789ABCDEF");
  (void)selected_signal_log_format_paths(
    identity.start_unix_ms, 480, identity.csv_path,
    sizeof(identity.csv_path), identity.meta_path,
    sizeof(identity.meta_path));
  return identity;
}

static bool test_admission_boundaries(void) {
  ASSERT_TRUE(selected_signal_log_admissible(1u, 100u));
  ASSERT_TRUE(selected_signal_log_admissible(2u, 100u));
  ASSERT_TRUE(!selected_signal_log_admissible(3u, 100u));
  ASSERT_TRUE(selected_signal_log_admissible(16u, 800u));
  ASSERT_TRUE(!selected_signal_log_admissible(16u, 799u));
  ASSERT_TRUE(selected_signal_log_admissible(64u, 3200u));
  ASSERT_TRUE(!selected_signal_log_admissible(64u, 3199u));
  ASSERT_TRUE(selected_signal_log_admissible(128u, 6400u));
  ASSERT_TRUE(!selected_signal_log_admissible(128u, 6399u));
  ASSERT_TRUE(selected_signal_log_admissible(128u, 10000u));
  ASSERT_TRUE(!selected_signal_log_admissible(0u, 10000u));
  ASSERT_TRUE(!selected_signal_log_admissible(129u, 10000u));
  ASSERT_TRUE(!selected_signal_log_admissible(1u, 99u));
  ASSERT_TRUE(!selected_signal_log_admissible(1u, 10001u));
  return true;
}

static bool test_paths_and_collision(void) {
  char csv[SELECTED_SIGNAL_LOG_PATH_MAX_BYTES];
  char meta[SELECTED_SIGNAL_LOG_PATH_MAX_BYTES];
  ASSERT_TRUE(selected_signal_log_format_paths(
    UINT64_C(1709251199123), 480, csv, sizeof(csv), meta, sizeof(meta)) ==
    SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(strcmp(csv,
    "/log/20240301_075959123_signal-v3.csv") == 0);
  ASSERT_TRUE(strcmp(meta,
    "/log/20240301_075959123_signal-v3.meta") == 0);
  ASSERT_TRUE(selected_signal_log_check_path_collision(false, false) ==
              SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(selected_signal_log_check_path_collision(true, false) ==
              SELECTED_SIGNAL_LOG_PATH_COLLISION);
  ASSERT_TRUE(selected_signal_log_check_path_collision(false, true) ==
              SELECTED_SIGNAL_LOG_PATH_COLLISION);
  ASSERT_TRUE(selected_signal_log_format_paths(
    UINT64_C(1709251199123), 480, csv, 8u, meta, sizeof(meta)) ==
    SELECTED_SIGNAL_LOG_CAPACITY);
  ASSERT_TRUE(csv[0] == '\0' && meta[0] == '\0');
  return true;
}

static bool test_session_state_and_preservation(void) {
  SelectedSignalLogSession session;
  SelectedSignalLogIdentity identity = make_identity();
  selected_signal_log_session_init(&session);
  SelectedSignalLogSession before = session;
  ASSERT_TRUE(selected_signal_log_session_prepare(
    &session, &identity, true, false) ==
    SELECTED_SIGNAL_LOG_PATH_COLLISION);
  ASSERT_TRUE(memcmp(&session, &before, sizeof(session)) == 0);

  SelectedSignalLogIdentity invalid = identity;
  invalid.sample_period_ms = 100u;
  ASSERT_TRUE(selected_signal_log_session_prepare(
    &session, &invalid, false, false) == SELECTED_SIGNAL_LOG_RATE_LIMIT);
  ASSERT_TRUE(memcmp(&session, &before, sizeof(session)) == 0);

  invalid = identity;
  strcpy(invalid.meta_path, "/log/unrelated.meta");
  ASSERT_TRUE(selected_signal_log_session_prepare(
    &session, &invalid, false, false) ==
    SELECTED_SIGNAL_LOG_INVALID_IDENTITY);
  ASSERT_TRUE(memcmp(&session, &before, sizeof(session)) == 0);

  invalid = identity;
  invalid.source_size = LARGE_DBC_SOURCE_MAX_BYTES + 1u;
  ASSERT_TRUE(selected_signal_log_session_prepare(
    &session, &invalid, false, false) ==
    SELECTED_SIGNAL_LOG_INVALID_IDENTITY);
  ASSERT_TRUE(memcmp(&session, &before, sizeof(session)) == 0);

  ASSERT_TRUE(selected_signal_log_session_prepare(
    &session, &identity, false, false) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(session.state == SELECTED_SIGNAL_LOG_STARTING &&
              memcmp(&session.identity, &identity, sizeof(identity)) == 0);
  ASSERT_TRUE(selected_signal_log_session_transition(
    &session, SELECTED_SIGNAL_LOG_STOPPING) ==
    SELECTED_SIGNAL_LOG_INVALID_STATE);
  ASSERT_TRUE(session.state == SELECTED_SIGNAL_LOG_STARTING);
  ASSERT_TRUE(selected_signal_log_session_transition(
    &session, SELECTED_SIGNAL_LOG_ACTIVE) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(selected_signal_log_session_transition(
    &session, SELECTED_SIGNAL_LOG_STOPPING) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(selected_signal_log_session_transition(
    &session, SELECTED_SIGNAL_LOG_STOPPED) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(selected_signal_log_session_prepare(
    &session, &identity, false, false) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(selected_signal_log_session_transition(
    &session, SELECTED_SIGNAL_LOG_FAILED) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(selected_signal_log_session_transition(
    &session, SELECTED_SIGNAL_LOG_STOPPED) == SELECTED_SIGNAL_LOG_OK);
  return true;
}

static bool test_quality_contract(void) {
  SignalValueSnapshot value = {
    .value = 1.0,
    .raw = 1,
    .updated_ms = UINT32_MAX - 1000u,
    .quality = SIGNAL_VALUE_QUALITY_GOOD
  };
  ASSERT_TRUE(selected_signal_log_effective_quality(&value, 1000u) ==
              SIGNAL_VALUE_QUALITY_GOOD);
  ASSERT_TRUE(selected_signal_log_effective_quality(&value, 2000u) ==
              SIGNAL_VALUE_QUALITY_STALE);
  value.quality = SIGNAL_VALUE_QUALITY_MISSING;
  ASSERT_TRUE(selected_signal_log_effective_quality(&value, 2000u) ==
              SIGNAL_VALUE_QUALITY_MISSING);
  value.quality = (SignalValueQuality)99;
  ASSERT_TRUE(selected_signal_log_effective_quality(&value, 2000u) ==
              SIGNAL_VALUE_QUALITY_ERROR);
  ASSERT_TRUE(selected_signal_log_effective_quality(NULL, 0u) ==
              SIGNAL_VALUE_QUALITY_ERROR);
  return true;
}

static bool test_csv_header_and_row(void) {
  char output[SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES];
  size_t length = 0u;
  DbcSelectedRuntimeSignal signal = {
    .catalog_ordinal = 17u,
    .definition_hash = UINT64_C(0x26A3283B4020769E)
  };
  strcpy(signal.key, "Message.quoted,\"signal\"\r\nnext");
  strcpy(signal.unit, "\"V\",\xE4\xBC\x8F");
  SignalValueSnapshot value = {
    .value = -12.5,
    .raw = INT64_MIN,
    .updated_ms = 7u,
    .quality = SIGNAL_VALUE_QUALITY_GOOD
  };

  ASSERT_TRUE(selected_signal_log_serialize_csv_header(
    output, sizeof(output), &length) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(strcmp(output,
    "utc_time,unix_ms,updated_ms,key,value,raw,unit,quality\n") == 0);
  ASSERT_TRUE(length == strlen(output));

  ASSERT_TRUE(selected_signal_log_serialize_csv_row(
    0u, 3007u, &signal, &value, output, sizeof(output), &length) ==
    SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(strcmp(output,
    "1970-01-01T00:00:00.000Z,0,7,\"Message.quoted,\"\"signal\"\"\r\nnext\",-12.5,-9223372036854775808,\"\"\"V\"\",\xE4\xBC\x8F\",GOOD\n") == 0);
  ASSERT_TRUE(length == strlen(output) && length < 512u);

  const size_t exact_length = length;
  ASSERT_TRUE(selected_signal_log_serialize_csv_row(
    0u, 3007u, &signal, &value, output, exact_length + 1u, &length) ==
    SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(length == exact_length);
  ASSERT_TRUE(selected_signal_log_serialize_csv_row(
    0u, 3007u, &signal, &value, output, exact_length, &length) ==
    SELECTED_SIGNAL_LOG_CAPACITY);
  ASSERT_TRUE(length == 0u && output[0] == '\0');

  ASSERT_TRUE(selected_signal_log_serialize_csv_row(
    0u, 3008u, &signal, &value, output, sizeof(output), &length) ==
    SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(strstr(output, ",STALE\n") != NULL);

  ASSERT_TRUE(selected_signal_log_serialize_csv_row(
    0u, 0u, &signal, &value, output, 32u, &length) ==
    SELECTED_SIGNAL_LOG_CAPACITY);
  ASSERT_TRUE(length == 0u && output[0] == '\0');
  value.value = INFINITY;
  ASSERT_TRUE(selected_signal_log_serialize_csv_row(
    0u, 0u, &signal, &value, output, sizeof(output), &length) ==
    SELECTED_SIGNAL_LOG_INVALID_VALUE);
  ASSERT_TRUE(length == 0u && output[0] == '\0');
  return true;
}

static bool test_worst_valid_csv_row_fits_scratch(void) {
  DbcSelectedRuntimeSignal signal = {0};
  memset(signal.key, '"', LARGE_DBC_KEY_MAX_BYTES);
  signal.key[LARGE_DBC_KEY_MAX_BYTES] = '\0';
  memset(signal.unit, '"', LARGE_DBC_UNIT_MAX_BYTES);
  signal.unit[LARGE_DBC_UNIT_MAX_BYTES] = '\0';
  SignalValueSnapshot value = {
    .value = 100000000000000000000.0,
    .raw = INT64_MIN,
    .updated_ms = UINT32_MAX,
    .quality = SIGNAL_VALUE_QUALITY_MISSING
  };
  char output[SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES];
  size_t length = 0u;
  ASSERT_TRUE(selected_signal_log_serialize_csv_row(
    UINT64_C(1709251199123), UINT32_MAX, &signal, &value,
    output, sizeof(output), &length) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(length > 0u && length < sizeof(output));
  return true;
}

static bool test_meta_stream_fragments(void) {
  SelectedSignalLogIdentity identity = make_identity();
  char output[SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES];
  size_t length = 0u;
  ASSERT_TRUE(selected_signal_log_serialize_meta_header(
    &identity, output, sizeof(output), &length) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(strstr(output, "format=selected-signal-log-meta-v1\n") == output);
  ASSERT_TRUE(strstr(output, "activeGeneration=0000000000000007\n") != NULL);
  ASSERT_TRUE(strstr(output, "sourceCrc32=4B88D9CE\n") != NULL);
  ASSERT_TRUE(strstr(output, "selectionCrc32=62BA0D66\n") != NULL);
  ASSERT_TRUE(strstr(output, "cleanClose=false\n") != NULL);
  ASSERT_TRUE(length == strlen(output) && length < 512u);

  DbcSelectedRuntimeSignal signal = {
    .catalog_ordinal = 127u,
    .definition_hash = UINT64_C(0x26A3283B4020769E)
  };
  strcpy(signal.key, "Message.quoted,\"signal\"");
  ASSERT_TRUE(selected_signal_log_serialize_meta_signal(
    &signal, output, sizeof(output), &length) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(strcmp(output,
    "signal=127,26A3283B4020769E,\"Message.quoted,\"\"signal\"\"\"\n") == 0);

  SelectedSignalLogCounters counters = {
    .rows_written = UINT64_C(18446744073709551615),
    .rows_dropped = 2u,
    .late_samples = 3u,
    .write_failures = 4u,
    .flush_count = 5u
  };
  ASSERT_TRUE(selected_signal_log_serialize_meta_footer(
    UINT64_C(1709251200000), true, &counters, output,
    sizeof(output), &length) == SELECTED_SIGNAL_LOG_OK);
  ASSERT_TRUE(strstr(output,
    "rowsWritten=18446744073709551615\n") != NULL);
  ASSERT_TRUE(strstr(output, "cleanClose=true\n") != NULL);

  ASSERT_TRUE(selected_signal_log_serialize_meta_header(
    &identity, output, 32u, &length) == SELECTED_SIGNAL_LOG_CAPACITY);
  ASSERT_TRUE(length == 0u && output[0] == '\0');
  return true;
}

static bool test_selected_cardinality_iteration(void) {
  static const uint16_t counts[] = {1u, 16u, 64u, 128u};
  char output[SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES];
  for (size_t test = 0u; test < sizeof(counts) / sizeof(counts[0]); ++test) {
    size_t rows = 0u;
    for (uint16_t ordinal = 0u; ordinal < counts[test]; ++ordinal) {
      DbcSelectedRuntimeSignal signal = {
        .catalog_ordinal = ordinal,
        .definition_hash = (uint64_t)ordinal + 1u
      };
      snprintf(signal.key, sizeof(signal.key), "M.S%u", (unsigned)ordinal);
      SignalValueSnapshot value = {
        .value = ordinal,
        .raw = ordinal,
        .updated_ms = ordinal,
        .quality = SIGNAL_VALUE_QUALITY_GOOD
      };
      size_t length = 0u;
      ASSERT_TRUE(selected_signal_log_serialize_csv_row(
        0u, ordinal, &signal, &value, output, sizeof(output), &length) ==
        SELECTED_SIGNAL_LOG_OK);
      ASSERT_TRUE(length != 0u && strstr(output, signal.key) != NULL);
      ++rows;
    }
    ASSERT_TRUE(rows == counts[test]);
  }
  return true;
}

int main(void) {
  if (!test_admission_boundaries() || !test_paths_and_collision() ||
      !test_session_state_and_preservation() || !test_quality_contract() ||
      !test_csv_header_and_row() || !test_worst_valid_csv_row_fits_scratch() ||
      !test_meta_stream_fragments() || !test_selected_cardinality_iteration()) {
    return 1;
  }
  puts("selected signal log tests passed");
  return 0;
}
