#include "dbc_active_commit.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition) do { if (!(condition)) { \
  fprintf(stderr, "ASSERT failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
  return false; } } while (0)

typedef enum {
  STEP_VERIFY_CANDIDATE = 1,
  STEP_CHECK_LOGGING,
  STEP_CHECK_RULES,
  STEP_BUILD_RUNTIME,
  STEP_VERIFY_RUNTIME,
  STEP_WRITE_SOURCE,
  STEP_WRITE_INDEX,
  STEP_WRITE_SELECTION,
  STEP_VERIFY_SOURCE,
  STEP_VERIFY_INDEX,
  STEP_VERIFY_SELECTION,
  STEP_CROSS_VERIFY,
  STEP_PUBLISH_MANIFEST,
  STEP_PUBLISH_RUNTIME
} MockStep;

typedef struct {
  MockStep steps[20];
  size_t count;
  MockStep fail;
  bool logging;
  DbcActiveRulesStatus rules;
  DbcActiveDescriptor persistent;
  DbcActiveDescriptor runtime;
} Mock;

static DbcCandidateDescriptor candidate(void) {
  const DbcCandidateDescriptor value = {
    .generation = 9u, .selection_generation = 9u,
    .source_size = 100071u, .source_crc32 = UINT32_C(0x4B88D9CE),
    .index_size = 145232u, .index_crc32 = UINT32_C(0x062F7437),
    .selection_size = 320u, .selection_crc32 = UINT32_C(0x12345678),
    .catalog_message_count = 112u, .catalog_signal_count = 896u,
    .selected_count = 8u, .selected_message_count = 1u
  };
  return value;
}

static bool record(Mock *mock, MockStep step) {
  mock->steps[mock->count++] = step;
  return mock->fail != step;
}

static bool verify_candidate(void *context, const DbcCandidateDescriptor *value) {
  (void)value;
  return record(context, STEP_VERIFY_CANDIDATE);
}
static bool logging_active(void *context) {
  Mock *mock = context;
  (void)record(mock, STEP_CHECK_LOGGING);
  return mock->logging;
}
static DbcActiveRulesStatus check_rules(void *context,
                                        const DbcCandidateDescriptor *value) {
  Mock *mock = context;
  (void)value;
  (void)record(mock, STEP_CHECK_RULES);
  return mock->rules;
}
static bool build_runtime(void *context, const DbcActiveDescriptor *value) {
  (void)value;
  return record(context, STEP_BUILD_RUNTIME);
}
static bool verify_runtime(void *context, const DbcActiveDescriptor *value) {
  (void)value;
  return record(context, STEP_VERIFY_RUNTIME);
}
static MockStep write_step(DbcCandidateObjectKind kind) {
  return (MockStep)((unsigned)STEP_WRITE_SOURCE + (unsigned)kind);
}
static MockStep verify_step(DbcCandidateObjectKind kind) {
  return (MockStep)((unsigned)STEP_VERIFY_SOURCE + (unsigned)kind);
}
static bool write_object(void *context, DbcCandidateObjectKind kind,
                         const DbcActiveDescriptor *value) {
  (void)value;
  return record(context, write_step(kind));
}
static bool verify_object(void *context, DbcCandidateObjectKind kind,
                          const DbcActiveDescriptor *value) {
  (void)value;
  return record(context, verify_step(kind));
}
static bool cross_verify(void *context, const DbcActiveDescriptor *value) {
  (void)value;
  return record(context, STEP_CROSS_VERIFY);
}
static bool publish_manifest(void *context, const DbcActiveDescriptor *value) {
  Mock *mock = context;
  if (!record(mock, STEP_PUBLISH_MANIFEST)) return false;
  mock->persistent = *value;
  return true;
}
static void publish_runtime(void *context, const DbcActiveDescriptor *value) {
  Mock *mock = context;
  (void)record(mock, STEP_PUBLISH_RUNTIME);
  mock->runtime = *value;
}

static DbcActiveCommitCallbacks callbacks(Mock *mock) {
  const DbcActiveCommitCallbacks value = {
    .context = mock, .verify_candidate = verify_candidate,
    .logging_is_active = logging_active, .check_rules = check_rules,
    .build_inactive_runtime = build_runtime,
    .verify_inactive_runtime = verify_runtime,
    .write_generation_object = write_object,
    .verify_generation_object = verify_object,
    .cross_verify_generation = cross_verify,
    .publish_current_manifest = publish_manifest,
    .publish_runtime = publish_runtime
  };
  return value;
}

static bool descriptor_equal(const DbcActiveDescriptor *a,
                             const DbcActiveDescriptor *b) {
  return memcmp(a, b, sizeof(*a)) == 0;
}

static bool test_success_order(void) {
  Mock mock = {.rules = DBC_ACTIVE_RULES_OK};
  DbcActiveDescriptor out = {0};
  const DbcCandidateDescriptor input = candidate();
  const DbcActiveCommitCallbacks cb = callbacks(&mock);
  ASSERT_TRUE(dbc_active_commit(3u, &input, &cb, &out) == DBC_ACTIVE_COMMIT_OK);
  static const MockStep expected[] = {
    STEP_VERIFY_CANDIDATE, STEP_CHECK_LOGGING, STEP_CHECK_RULES,
    STEP_BUILD_RUNTIME, STEP_VERIFY_RUNTIME,
    STEP_WRITE_SOURCE, STEP_WRITE_INDEX, STEP_WRITE_SELECTION,
    STEP_VERIFY_SOURCE, STEP_VERIFY_INDEX, STEP_VERIFY_SELECTION,
    STEP_CROSS_VERIFY, STEP_PUBLISH_MANIFEST, STEP_PUBLISH_RUNTIME
  };
  ASSERT_TRUE(mock.count == sizeof(expected) / sizeof(expected[0]));
  ASSERT_TRUE(memcmp(mock.steps, expected, sizeof(expected)) == 0);
  ASSERT_TRUE(out.active_generation == 3u && out.candidate_generation == 9u &&
              out.selection_generation == 9u && out.selected_count == 8u);
  ASSERT_TRUE(descriptor_equal(&out, &mock.persistent));
  ASSERT_TRUE(descriptor_equal(&out, &mock.runtime));
  return true;
}

static bool test_all_persistent_faults_keep_old(void) {
  static const struct { MockStep step; DbcActiveCommitStatus status; } cases[] = {
    {STEP_VERIFY_CANDIDATE, DBC_ACTIVE_COMMIT_CANDIDATE_VERIFY_FAILED},
    {STEP_BUILD_RUNTIME, DBC_ACTIVE_COMMIT_RUNTIME_BUILD_FAILED},
    {STEP_VERIFY_RUNTIME, DBC_ACTIVE_COMMIT_RUNTIME_VERIFY_FAILED},
    {STEP_WRITE_SOURCE, DBC_ACTIVE_COMMIT_SOURCE_WRITE_FAILED},
    {STEP_WRITE_INDEX, DBC_ACTIVE_COMMIT_INDEX_WRITE_FAILED},
    {STEP_WRITE_SELECTION, DBC_ACTIVE_COMMIT_SELECTION_WRITE_FAILED},
    {STEP_VERIFY_SOURCE, DBC_ACTIVE_COMMIT_SOURCE_READBACK_FAILED},
    {STEP_VERIFY_INDEX, DBC_ACTIVE_COMMIT_INDEX_READBACK_FAILED},
    {STEP_VERIFY_SELECTION, DBC_ACTIVE_COMMIT_SELECTION_READBACK_FAILED},
    {STEP_CROSS_VERIFY, DBC_ACTIVE_COMMIT_CROSS_VERIFY_FAILED},
    {STEP_PUBLISH_MANIFEST, DBC_ACTIVE_COMMIT_MANIFEST_PUBLISH_FAILED}
  };
  const DbcCandidateDescriptor input = candidate();
  for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    Mock mock = {.fail = cases[i].step, .rules = DBC_ACTIVE_RULES_OK};
    mock.persistent.active_generation = 2u;
    mock.runtime.active_generation = 2u;
    const DbcActiveDescriptor old_persistent = mock.persistent;
    const DbcActiveDescriptor old_runtime = mock.runtime;
    DbcActiveDescriptor out = {.active_generation = 77u};
    const DbcActiveCommitCallbacks cb = callbacks(&mock);
    ASSERT_TRUE(dbc_active_commit(3u, &input, &cb, &out) == cases[i].status);
    ASSERT_TRUE(descriptor_equal(&mock.persistent, &old_persistent));
    ASSERT_TRUE(descriptor_equal(&mock.runtime, &old_runtime));
    ASSERT_TRUE(out.active_generation == 77u);
  }
  return true;
}

static bool test_gates(void) {
  DbcCandidateDescriptor input = candidate();
  DbcActiveDescriptor out = {0};
  Mock mock = {.logging = true, .rules = DBC_ACTIVE_RULES_OK};
  DbcActiveCommitCallbacks cb = callbacks(&mock);
  ASSERT_TRUE(dbc_active_commit(1u, &input, &cb, &out) ==
              DBC_ACTIVE_COMMIT_LOGGING_ACTIVE);
  ASSERT_TRUE(mock.count == 2u);

  mock = (Mock){.rules = DBC_ACTIVE_RULES_KEY_MISSING};
  cb = callbacks(&mock);
  ASSERT_TRUE(dbc_active_commit(1u, &input, &cb, &out) ==
              DBC_ACTIVE_COMMIT_RULE_KEY_MISSING);
  mock = (Mock){.rules = DBC_ACTIVE_RULES_DEFINITION_CONFLICT};
  cb = callbacks(&mock);
  ASSERT_TRUE(dbc_active_commit(1u, &input, &cb, &out) ==
              DBC_ACTIVE_COMMIT_RULE_DEFINITION_CONFLICT);

  input.selected_count = 0u;
  input.selected_message_count = 0u;
  mock = (Mock){.rules = DBC_ACTIVE_RULES_OK};
  cb = callbacks(&mock);
  ASSERT_TRUE(dbc_active_commit(1u, &input, &cb, &out) ==
              DBC_ACTIVE_COMMIT_EMPTY_SELECTION);
  input = candidate();
  input.selected_count = 129u;
  ASSERT_TRUE(dbc_active_commit(1u, &input, &cb, &out) ==
              DBC_ACTIVE_COMMIT_SELECTION_LIMIT);
  input = candidate();
  input.selected_message_count = 65u;
  ASSERT_TRUE(dbc_active_commit(1u, &input, &cb, &out) ==
              DBC_ACTIVE_COMMIT_SELECTION_LIMIT);
  return true;
}

int main(void) {
  if (!test_success_order() || !test_all_persistent_faults_keep_old() ||
      !test_gates()) return 1;
  puts("dbc active commit tests passed");
  return 0;
}
