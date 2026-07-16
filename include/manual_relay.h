#ifndef MANUAL_RELAY_H
#define MANUAL_RELAY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t enabled;
  uint32_t relay1;
  uint32_t relay2;
  uint32_t request_seq;
  uint32_t applied_seq;
} ManualRelayState;

bool manual_relay_state_submit(ManualRelayState *state,
                               uint32_t enabled,
                               uint32_t relay1,
                               uint32_t relay2);
void manual_relay_state_mark_applied(ManualRelayState *state, uint32_t request_seq);

#endif
