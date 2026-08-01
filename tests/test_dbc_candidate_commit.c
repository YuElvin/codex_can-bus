#include "dbc_candidate_commit.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition) do { if (!(condition)) { \
  fprintf(stderr, "ASSERT failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
  return false; } } while (0)

typedef enum {
  STEP_BUILD_INDEX = 1,
  STEP_BUILD_SELECTION,
  STEP_WRITE_SOURCE,
  STEP_WRITE_INDEX,
  STEP_WRITE_SELECTION,
  STEP_VERIFY_SOURCE,
  STEP_VERIFY_INDEX,
  STEP_VERIFY_SELECTION,
  STEP_CROSS_VERIFY,
  STEP_PUBLISH_MANIFEST
} MockStep;

typedef struct {
  MockStep steps[16];
  size_t step_count;
  MockStep fail_step;
  DbcCandidateIndexBuildResult index_result;
  DbcCandidateDescriptor committed_current;
  uint16_t message_count;
  uint16_t signal_count;
  bool corrupt_source_identity;
  bool corrupt_default_selection;
} MockCommit;

static DbcCandidateDescriptor old_candidate(void) {
  const DbcCandidateDescriptor descriptor = {
    .generation = 7u,
    .selection_generation = 7u,
    .source_size = 151u,
    .source_crc32 = UINT32_C(0xF2852B8E),
    .index_size = 416u,
    .index_crc32 = UINT32_C(0x11111111),
    .selection_size = 320u,
    .selection_crc32 = UINT32_C(0x22222222),
    .catalog_message_count = 1u,
    .catalog_signal_count = 2u,
    .selected_count = 2u,
    .selected_message_count = 1u
  };
  return descriptor;
}

static bool descriptor_equal(const DbcCandidateDescriptor *left,
                             const DbcCandidateDescriptor *right) {
  return left->generation == right->generation &&
         left->selection_generation == right->selection_generation &&
         left->source_size == right->source_size &&
         left->source_crc32 == right->source_crc32 &&
         left->index_size == right->index_size &&
         left->index_crc32 == right->index_crc32 &&
         left->selection_size == right->selection_size &&
         left->selection_crc32 == right->selection_crc32 &&
         left->catalog_message_count == right->catalog_message_count &&
         left->catalog_signal_count == right->catalog_signal_count &&
         left->selected_count == right->selected_count &&
         left->selected_message_count == right->selected_message_count;
}

static bool record_step(MockCommit *mock, MockStep step) {
  mock->steps[mock->step_count++] = step;
  return mock->fail_step != step;
}

static DbcCandidateIndexBuildResult mock_build_index(
  void *context,
  const DbcCandidateUpload *upload,
  DbcCandidateDescriptor *candidate) {
  MockCommit *mock = context;
  (void)upload;
  (void)record_step(mock, STEP_BUILD_INDEX);
  if (mock->index_result != DBC_CANDIDATE_INDEX_BUILD_OK) {
    return mock->index_result;
  }
  candidate->index_size = 145232u;
  candidate->index_crc32 = UINT32_C(0x1234ABCD);
  candidate->catalog_message_count = mock->message_count;
  candidate->catalog_signal_count = mock->signal_count;
  if (mock->corrupt_source_identity) {
    ++candidate->source_size;
  }
  return DBC_CANDIDATE_INDEX_BUILD_OK;
}

static bool mock_build_selection(void *context,
                                 DbcCandidateDescriptor *candidate) {
  MockCommit *mock = context;
  if (!record_step(mock, STEP_BUILD_SELECTION)) {
    return false;
  }
  candidate->selection_generation = candidate->generation;
  candidate->selection_size = 320u;
  candidate->selection_crc32 = UINT32_C(0x89ABCDEF);
  if (candidate->catalog_signal_count <= LARGE_DBC_ACTIVE_MAX_SIGNALS) {
    candidate->selected_count = candidate->catalog_signal_count;
    candidate->selected_message_count = candidate->catalog_message_count;
  } else {
    candidate->selected_count = 0u;
    candidate->selected_message_count = 0u;
  }
  if (mock->corrupt_default_selection) {
    candidate->selected_count = 1u;
    candidate->selected_message_count = 1u;
  }
  return true;
}

static MockStep write_step(DbcCandidateObjectKind kind) {
  return (MockStep)((unsigned)STEP_WRITE_SOURCE + (unsigned)kind);
}

static MockStep verify_step(DbcCandidateObjectKind kind) {
  return (MockStep)((unsigned)STEP_VERIFY_SOURCE + (unsigned)kind);
}

static bool mock_write(void *context, DbcCandidateObjectKind kind,
                       const DbcCandidateDescriptor *candidate) {
  MockCommit *mock = context;
  (void)candidate;
  return record_step(mock, write_step(kind));
}

static bool mock_verify(void *context, DbcCandidateObjectKind kind,
                        const DbcCandidateDescriptor *candidate) {
  MockCommit *mock = context;
  (void)candidate;
  return record_step(mock, verify_step(kind));
}

static bool mock_cross_verify(void *context,
                              const DbcCandidateDescriptor *candidate) {
  MockCommit *mock = context;
  (void)candidate;
  return record_step(mock, STEP_CROSS_VERIFY);
}

static bool mock_publish(void *context,
                         const DbcCandidateDescriptor *candidate) {
  MockCommit *mock = context;
  if (!record_step(mock, STEP_PUBLISH_MANIFEST)) {
    return false;
  }
  mock->committed_current = *candidate;
  return true;
}

static DbcCandidateCommitCallbacks callbacks_for(MockCommit *mock) {
  const DbcCandidateCommitCallbacks callbacks = {
    .context = mock,
    .build_index_from_upload = mock_build_index,
    .build_default_selection = mock_build_selection,
    .write_generation_object = mock_write,
    .verify_generation_object = mock_verify,
    .cross_verify_generation = mock_cross_verify,
    .publish_current_manifest = mock_publish
  };
  return callbacks;
}

static void mock_init(MockCommit *mock) {
  memset(mock, 0, sizeof(*mock));
  mock->index_result = DBC_CANDIDATE_INDEX_BUILD_OK;
  mock->message_count = 112u;
  mock->signal_count = 896u;
  mock->committed_current = old_candidate();
}

static DbcCandidateUpload large_upload(void) {
  const DbcCandidateUpload upload = {
    .generation = UINT64_C(0x1020304050607080),
    .source_size = 100071u,
    .source_crc32 = UINT32_C(0x4B88D9CE)
  };
  return upload;
}

static bool test_success_order_and_publish_boundary(void) {
  MockCommit mock;
  mock_init(&mock);
  const DbcCandidateDescriptor old = mock.committed_current;
  DbcCandidateDescriptor published = old;
  const DbcCandidateUpload upload = large_upload();
  const DbcCandidateCommitCallbacks callbacks = callbacks_for(&mock);
  ASSERT_TRUE(dbc_candidate_commit(&upload, &callbacks, &published) ==
              DBC_CANDIDATE_COMMIT_OK);
  static const MockStep expected[] = {
    STEP_BUILD_INDEX, STEP_BUILD_SELECTION,
    STEP_WRITE_SOURCE, STEP_WRITE_INDEX, STEP_WRITE_SELECTION,
    STEP_VERIFY_SOURCE, STEP_VERIFY_INDEX, STEP_VERIFY_SELECTION,
    STEP_CROSS_VERIFY, STEP_PUBLISH_MANIFEST
  };
  ASSERT_TRUE(mock.step_count == sizeof(expected) / sizeof(expected[0]));
  ASSERT_TRUE(memcmp(mock.steps, expected, sizeof(expected)) == 0);
  ASSERT_TRUE(published.generation == upload.generation &&
              published.source_size == upload.source_size &&
              published.source_crc32 == upload.source_crc32);
  ASSERT_TRUE(published.catalog_message_count == 112u &&
              published.catalog_signal_count == 896u &&
              published.selected_count == 0u &&
              published.selected_message_count == 0u);
  ASSERT_TRUE(descriptor_equal(&published, &mock.committed_current));
  ASSERT_TRUE(!descriptor_equal(&old, &mock.committed_current));
  return true;
}

static bool test_index_pipeline_failures_keep_old_current(void) {
  static const struct {
    DbcCandidateIndexBuildResult build;
    DbcCandidateCommitStatus expected;
  } cases[] = {
    {DBC_CANDIDATE_INDEX_SOURCE_READ_FAILED,
     DBC_CANDIDATE_COMMIT_SOURCE_READ_FAILED},
    {DBC_CANDIDATE_INDEX_PARSE_FAILED, DBC_CANDIDATE_COMMIT_PARSE_FAILED},
    {DBC_CANDIDATE_INDEX_WRITE_OR_VERIFY_FAILED,
     DBC_CANDIDATE_COMMIT_INDEX_FAILED}
  };
  for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    MockCommit mock;
    mock_init(&mock);
    mock.index_result = cases[i].build;
    const DbcCandidateDescriptor old = mock.committed_current;
    DbcCandidateDescriptor published = old;
    const DbcCandidateUpload upload = large_upload();
    const DbcCandidateCommitCallbacks callbacks = callbacks_for(&mock);
    ASSERT_TRUE(dbc_candidate_commit(&upload, &callbacks, &published) ==
                cases[i].expected);
    ASSERT_TRUE(mock.step_count == 1u && mock.steps[0] == STEP_BUILD_INDEX);
    ASSERT_TRUE(descriptor_equal(&published, &old));
    ASSERT_TRUE(descriptor_equal(&mock.committed_current, &old));
  }
  return true;
}

static bool test_all_later_faults_keep_old_current(void) {
  static const struct {
    MockStep fail;
    DbcCandidateCommitStatus expected;
  } cases[] = {
    {STEP_BUILD_SELECTION, DBC_CANDIDATE_COMMIT_SELECTION_FAILED},
    {STEP_WRITE_SOURCE, DBC_CANDIDATE_COMMIT_SOURCE_WRITE_FAILED},
    {STEP_WRITE_INDEX, DBC_CANDIDATE_COMMIT_INDEX_WRITE_FAILED},
    {STEP_WRITE_SELECTION, DBC_CANDIDATE_COMMIT_SELECTION_WRITE_FAILED},
    {STEP_VERIFY_SOURCE, DBC_CANDIDATE_COMMIT_SOURCE_READBACK_FAILED},
    {STEP_VERIFY_INDEX, DBC_CANDIDATE_COMMIT_INDEX_READBACK_FAILED},
    {STEP_VERIFY_SELECTION, DBC_CANDIDATE_COMMIT_SELECTION_READBACK_FAILED},
    {STEP_CROSS_VERIFY, DBC_CANDIDATE_COMMIT_CROSS_VERIFY_FAILED},
    {STEP_PUBLISH_MANIFEST, DBC_CANDIDATE_COMMIT_MANIFEST_PUBLISH_FAILED}
  };
  for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    MockCommit mock;
    mock_init(&mock);
    mock.fail_step = cases[i].fail;
    const DbcCandidateDescriptor old = mock.committed_current;
    DbcCandidateDescriptor published = old;
    const DbcCandidateUpload upload = large_upload();
    const DbcCandidateCommitCallbacks callbacks = callbacks_for(&mock);
    ASSERT_TRUE(dbc_candidate_commit(&upload, &callbacks, &published) ==
                cases[i].expected);
    ASSERT_TRUE(mock.steps[mock.step_count - 1u] == cases[i].fail);
    ASSERT_TRUE(descriptor_equal(&published, &old));
    ASSERT_TRUE(descriptor_equal(&mock.committed_current, &old));
  }
  return true;
}

static bool test_descriptor_invariants_and_small_default(void) {
  MockCommit mock;
  mock_init(&mock);
  mock.corrupt_source_identity = true;
  DbcCandidateDescriptor published = old_candidate();
  const DbcCandidateDescriptor old = published;
  const DbcCandidateUpload upload = large_upload();
  DbcCandidateCommitCallbacks callbacks = callbacks_for(&mock);
  ASSERT_TRUE(dbc_candidate_commit(&upload, &callbacks, &published) ==
              DBC_CANDIDATE_COMMIT_DESCRIPTOR_INVALID);
  ASSERT_TRUE(mock.step_count == 1u && descriptor_equal(&published, &old));

  mock_init(&mock);
  mock.corrupt_default_selection = true;
  callbacks = callbacks_for(&mock);
  ASSERT_TRUE(dbc_candidate_commit(&upload, &callbacks, &published) ==
              DBC_CANDIDATE_COMMIT_DESCRIPTOR_INVALID);
  ASSERT_TRUE(mock.step_count == 2u && descriptor_equal(&published, &old));

  mock_init(&mock);
  mock.message_count = 1u;
  mock.signal_count = 2u;
  callbacks = callbacks_for(&mock);
  ASSERT_TRUE(dbc_candidate_commit(&upload, &callbacks, &published) ==
              DBC_CANDIDATE_COMMIT_OK);
  ASSERT_TRUE(published.selected_count == 2u &&
              published.selected_message_count == 1u);
  return true;
}

static bool test_recovery_current_then_previous(void) {
  DbcCandidateRecoverySlot current = {
    .state = DBC_CANDIDATE_SLOT_VALID,
    .descriptor = old_candidate()
  };
  DbcCandidateRecoverySlot previous = {
    .state = DBC_CANDIDATE_SLOT_VALID,
    .descriptor = old_candidate()
  };
  previous.descriptor.generation = 6u;
  previous.descriptor.selection_generation = 6u;
  DbcCandidateDescriptor recovered;
  memset(&recovered, 0xA5, sizeof(recovered));
  ASSERT_TRUE(dbc_candidate_recovery_choose(&current, &previous, &recovered) ==
              DBC_CANDIDATE_RECOVERY_CURRENT);
  ASSERT_TRUE(recovered.generation == 7u);

  current.state = DBC_CANDIDATE_SLOT_INVALID;
  ASSERT_TRUE(dbc_candidate_recovery_choose(&current, &previous, &recovered) ==
              DBC_CANDIDATE_RECOVERY_PREVIOUS);
  ASSERT_TRUE(recovered.generation == 6u);

  current.state = DBC_CANDIDATE_SLOT_ABSENT;
  ASSERT_TRUE(dbc_candidate_recovery_choose(&current, &previous, &recovered) ==
              DBC_CANDIDATE_RECOVERY_PREVIOUS);
  current.state = DBC_CANDIDATE_SLOT_VALID;
  current.descriptor.index_size = 0u;
  ASSERT_TRUE(dbc_candidate_recovery_choose(&current, &previous, &recovered) ==
              DBC_CANDIDATE_RECOVERY_PREVIOUS);

  previous.state = DBC_CANDIDATE_SLOT_INVALID;
  const DbcCandidateDescriptor sentinel = recovered;
  ASSERT_TRUE(dbc_candidate_recovery_choose(&current, &previous, &recovered) ==
              DBC_CANDIDATE_RECOVERY_NONE);
  ASSERT_TRUE(descriptor_equal(&recovered, &sentinel));
  ASSERT_TRUE(dbc_candidate_recovery_choose(&current, &previous, NULL) ==
              DBC_CANDIDATE_RECOVERY_NONE);
  return true;
}

int main(void) {
  if (!test_success_order_and_publish_boundary() ||
      !test_index_pipeline_failures_keep_old_current() ||
      !test_all_later_faults_keep_old_current() ||
      !test_descriptor_invariants_and_small_default() ||
      !test_recovery_current_then_previous()) {
    return 1;
  }
  puts("dbc candidate commit tests passed");
  return 0;
}
