#ifndef CAN_TX_CONTROL_H
#define CAN_TX_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  bool enabled;
  uint16_t standard_id;
  uint8_t dlc;
  uint8_t data[8];
  uint32_t period_ms;
} CanTxControlConfig;

typedef struct {
  CanTxControlConfig config;
  uint32_t request_seq;
  uint32_t applied_seq;
  uint32_t last_result;
} CanTxControlState;

void can_tx_control_init(CanTxControlState *state);
bool can_tx_control_parse_form(const char *body, size_t body_len, CanTxControlConfig *config);
void can_tx_control_submit(CanTxControlState *state,
                           const CanTxControlConfig *config,
                           uint32_t *request_seq);

#endif
