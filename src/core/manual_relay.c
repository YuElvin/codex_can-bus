#include "manual_relay.h"

#include <stddef.h>

bool manual_relay_state_submit(ManualRelayState *state,
                               uint32_t enabled,
                               uint32_t relay1,
                               uint32_t relay2) {
  if (state == NULL || enabled > 1u || relay1 > 1u || relay2 > 1u) {
    return false;
  }

  state->enabled = enabled;
  state->relay1 = relay1;
  state->relay2 = relay2;
  ++state->request_seq;
  if (state->request_seq == 0u) {
    state->request_seq = 1u;
  }
  return true;
}

void manual_relay_state_mark_applied(ManualRelayState *state, uint32_t request_seq) {
  if (state != NULL) {
    state->applied_seq = request_seq;
  }
}
