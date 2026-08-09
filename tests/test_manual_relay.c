#include "manual_relay.h"

#include <stdint.h>
#include <stdio.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);              \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_submit_requires_complete_binary_values(void) {
  ManualRelayState state = {0};

  ASSERT_TRUE(manual_relay_state_submit(&state, 1u, 0u, 1u));
  ASSERT_TRUE(state.enabled == 1u);
  ASSERT_TRUE(state.relay1 == 0u);
  ASSERT_TRUE(state.relay2 == 1u);
  ASSERT_TRUE(state.request_seq == 1u);
  ASSERT_TRUE(!manual_relay_state_submit(&state, 2u, 0u, 1u));
  ASSERT_TRUE(!manual_relay_state_submit(&state, 1u, 2u, 1u));
  ASSERT_TRUE(!manual_relay_state_submit(&state, 1u, 0u, 2u));
  ASSERT_TRUE(state.request_seq == 1u);
  return 0;
}

static int test_applied_sequence_tracks_the_rule_task_cycle(void) {
  ManualRelayState state = {0};

  ASSERT_TRUE(manual_relay_state_submit(&state, 1u, 1u, 0u));
  manual_relay_state_mark_applied(&state, state.request_seq);
  ASSERT_TRUE(state.applied_seq == 1u);
  ASSERT_TRUE(manual_relay_state_submit(&state, 0u, 0u, 0u));
  ASSERT_TRUE(state.request_seq == 2u);
  ASSERT_TRUE(state.applied_seq == 1u);
  return 0;
}

static int test_request_sequence_skips_zero(void) {
  ManualRelayState state = {.request_seq = UINT32_MAX};

  ASSERT_TRUE(manual_relay_state_submit(&state, 0u, 0u, 0u));
  ASSERT_TRUE(state.request_seq == 1u);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_submit_requires_complete_binary_values() == 0);
  ASSERT_TRUE(test_applied_sequence_tracks_the_rule_task_cycle() == 0);
  ASSERT_TRUE(test_request_sequence_skips_zero() == 0);
  return 0;
}
