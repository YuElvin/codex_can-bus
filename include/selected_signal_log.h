#ifndef SELECTED_SIGNAL_LOG_H
#define SELECTED_SIGNAL_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dbc_selected_runtime.h"
#include "large_dbc_contract.h"

#define SELECTED_SIGNAL_LOG_META_FORMAT_VERSION 1u
#define SELECTED_SIGNAL_LOG_CSV_FORMAT_VERSION 4u
#define SELECTED_SIGNAL_LOG_STALE_AFTER_MS 3000u
#define SELECTED_SIGNAL_LOG_OUTPUT_MAX_BYTES 512u
#define SELECTED_SIGNAL_LOG_BUILD_ID_MAX_BYTES 64u
#define SELECTED_SIGNAL_LOG_PATH_MAX_BYTES 64u

typedef enum {
  SELECTED_SIGNAL_LOG_OK = 0,
  SELECTED_SIGNAL_LOG_INVALID_ARGUMENT,
  SELECTED_SIGNAL_LOG_RATE_LIMIT,
  SELECTED_SIGNAL_LOG_PATH_COLLISION,
  SELECTED_SIGNAL_LOG_INVALID_IDENTITY,
  SELECTED_SIGNAL_LOG_INVALID_STATE,
  SELECTED_SIGNAL_LOG_CAPACITY,
  SELECTED_SIGNAL_LOG_INVALID_VALUE
} SelectedSignalLogStatus;

typedef enum {
  SELECTED_SIGNAL_LOG_STOPPED = 0,
  SELECTED_SIGNAL_LOG_STARTING,
  SELECTED_SIGNAL_LOG_ACTIVE,
  SELECTED_SIGNAL_LOG_STOPPING,
  SELECTED_SIGNAL_LOG_FAILED
} SelectedSignalLogState;

typedef struct {
  uint16_t meta_format_version;
  uint16_t csv_format_version;
  uint16_t selected_count;
  uint16_t reserved;
  uint64_t active_generation;
  uint64_t candidate_generation;
  uint32_t source_size;
  uint32_t source_crc32;
  uint32_t selection_crc32;
  uint32_t sample_period_ms;
  uint64_t start_unix_ms;
  char firmware_build_id[SELECTED_SIGNAL_LOG_BUILD_ID_MAX_BYTES + 1u];
  char csv_path[SELECTED_SIGNAL_LOG_PATH_MAX_BYTES];
  char meta_path[SELECTED_SIGNAL_LOG_PATH_MAX_BYTES];
} SelectedSignalLogIdentity;

typedef struct {
  uint64_t rows_written;
  uint64_t rows_dropped;
  uint64_t late_samples;
  uint64_t write_failures;
  uint64_t flush_count;
} SelectedSignalLogCounters;

typedef struct {
  uint16_t session_format_version;
  uint8_t state;
  uint8_t reserved;
  SelectedSignalLogIdentity identity;
  SelectedSignalLogCounters counters;
} SelectedSignalLogSession;

bool selected_signal_log_admissible(uint16_t selected_count,
                                    uint32_t sample_period_ms);

SelectedSignalLogStatus selected_signal_log_format_paths(
  uint64_t start_unix_ms,
  int32_t utc_offset_min,
  char *csv_path,
  size_t csv_path_capacity,
  char *meta_path,
  size_t meta_path_capacity);

SelectedSignalLogStatus selected_signal_log_check_path_collision(
  bool csv_exists,
  bool meta_exists);

void selected_signal_log_session_init(SelectedSignalLogSession *session);

SelectedSignalLogStatus selected_signal_log_session_prepare(
  SelectedSignalLogSession *session,
  const SelectedSignalLogIdentity *identity,
  bool csv_exists,
  bool meta_exists);

SelectedSignalLogStatus selected_signal_log_session_transition(
  SelectedSignalLogSession *session,
  SelectedSignalLogState next_state);

SignalValueQuality selected_signal_log_effective_quality(
  const SignalValueSnapshot *value,
  uint32_t now_ms);

/* CSV v4 is a wide table.  The caller streams these small fragments because
 * a valid 128-signal header or sample row does not fit in one scratch buffer. */
SelectedSignalLogStatus selected_signal_log_serialize_csv_header_start(
  char *output,
  size_t output_capacity,
  size_t *output_length);

SelectedSignalLogStatus selected_signal_log_serialize_csv_header_signal(
  const DbcSelectedRuntimeSignal *signal,
  char *output,
  size_t output_capacity,
  size_t *output_length);

SelectedSignalLogStatus selected_signal_log_serialize_csv_row_start(
  uint64_t unix_ms,
  char *output,
  size_t output_capacity,
  size_t *output_length);

SelectedSignalLogStatus selected_signal_log_serialize_csv_row_value(
  uint32_t now_ms,
  const SignalValueSnapshot *value,
  char *output,
  size_t output_capacity,
  size_t *output_length);

SelectedSignalLogStatus selected_signal_log_serialize_csv_line_end(
  char *output,
  size_t output_capacity,
  size_t *output_length);

SelectedSignalLogStatus selected_signal_log_serialize_meta_header(
  const SelectedSignalLogIdentity *identity,
  char *output,
  size_t output_capacity,
  size_t *output_length);

SelectedSignalLogStatus selected_signal_log_serialize_meta_signal(
  const DbcSelectedRuntimeSignal *signal,
  char *output,
  size_t output_capacity,
  size_t *output_length);

SelectedSignalLogStatus selected_signal_log_serialize_meta_footer(
  uint64_t end_unix_ms,
  bool clean_close,
  const SelectedSignalLogCounters *counters,
  char *output,
  size_t output_capacity,
  size_t *output_length);

const char *selected_signal_log_status_string(SelectedSignalLogStatus status);

#endif
