#include "can_tx_control.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);              \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

int main(void) {
  CanTxControlState state;
  CanTxControlConfig config;
  uint32_t request_seq = 0u;

  can_tx_control_init(&state);
  ASSERT_TRUE(state.config.enabled);
  ASSERT_TRUE(state.config.standard_id == 0x321u);
  ASSERT_TRUE(state.config.dlc == 8u);
  ASSERT_TRUE(memcmp(state.config.data, "\xc2\xa5\x00\x01\x02\x03\x04\x05", 8u) == 0);
  ASSERT_TRUE(state.config.period_ms == 1000u);
  ASSERT_TRUE(state.request_seq == 0u && state.applied_seq == 0u && state.last_result == 0u);

  ASSERT_TRUE(can_tx_control_parse_form("enabled=1&id=801&dlc=3&data=1234567890aBcDeF&periodMs=250",
                                        strlen("enabled=1&id=801&dlc=3&data=1234567890aBcDeF&periodMs=250"),
                                        &config));
  ASSERT_TRUE(config.enabled && config.standard_id == 801u && config.dlc == 3u);
  ASSERT_TRUE(config.data[0] == 0x12u && config.data[1] == 0x34u && config.data[2] == 0x56u);
  ASSERT_TRUE(config.data[3] == 0u && config.data[7] == 0u && config.period_ms == 250u);

  ASSERT_TRUE(!can_tx_control_parse_form("enabled=1&id=801&dlc=8&data=1234&periodMs=250",
                                         strlen("enabled=1&id=801&dlc=8&data=1234&periodMs=250"),
                                         &config));
  ASSERT_TRUE(!can_tx_control_parse_form("enabled=1&id=2048&dlc=8&data=1234567890abcdef&periodMs=250",
                                         strlen("enabled=1&id=2048&dlc=8&data=1234567890abcdef&periodMs=250"),
                                         &config));
  ASSERT_TRUE(!can_tx_control_parse_form("enabled=1&id=1&dlc=8&data=1234567890abcdef&periodMs=99",
                                         strlen("enabled=1&id=1&dlc=8&data=1234567890abcdef&periodMs=99"),
                                         &config));
  ASSERT_TRUE(!can_tx_control_parse_form("enabled=1&id=1&dlc=8&data=1234567890abcdef&periodMs=100&x=1",
                                         strlen("enabled=1&id=1&dlc=8&data=1234567890abcdef&periodMs=100&x=1"),
                                         &config));
  ASSERT_TRUE(can_tx_control_parse_form("enabled=0&id=0&dlc=0&data=ffffffffffffffff&periodMs=10000",
                                        strlen("enabled=0&id=0&dlc=0&data=ffffffffffffffff&periodMs=10000"),
                                        &config));
  ASSERT_TRUE(!config.enabled && config.standard_id == 0u && config.dlc == 0u);
  ASSERT_TRUE(config.data[0] == 0u && config.data[7] == 0u && config.period_ms == 10000u);

  can_tx_control_submit(&state, &config, &request_seq);
  ASSERT_TRUE(request_seq == 1u && state.request_seq == 1u && state.last_result == UINT32_MAX);
  ASSERT_TRUE(state.config.standard_id == config.standard_id && state.config.period_ms == config.period_ms);
  return 0;
}
