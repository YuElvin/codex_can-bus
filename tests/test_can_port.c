#include "ports/can_port.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

typedef struct {
  bool configured;
  bool started;
  CanFrame rx_frame;
  bool has_rx;
} FakeCan;

static CanPortResult fake_configure(void *ctx, const CanPortConfig *config) {
  FakeCan *fake = (FakeCan *)ctx;
  fake->configured = config->nominal_bitrate == 500000u;
  return fake->configured ? CAN_PORT_OK : CAN_PORT_ERROR;
}

static CanPortResult fake_start(void *ctx) {
  FakeCan *fake = (FakeCan *)ctx;
  fake->started = fake->configured;
  return fake->started ? CAN_PORT_OK : CAN_PORT_ERROR;
}

static CanPortResult fake_send(void *ctx, const CanFrame *frame) {
  FakeCan *fake = (FakeCan *)ctx;
  if (!fake->started || frame->dlc > CAN_FRAME_MAX_DATA_LEN) {
    return CAN_PORT_ERROR;
  }
  return CAN_PORT_OK;
}

static CanPortResult fake_receive(void *ctx, CanFrame *frame) {
  FakeCan *fake = (FakeCan *)ctx;
  if (!fake->has_rx) {
    return CAN_PORT_RX_EMPTY;
  }
  *frame = fake->rx_frame;
  fake->has_rx = false;
  return CAN_PORT_OK;
}

static CanPortResult fake_status(void *ctx, CanPortStatus *status) {
  (void)ctx;
  memset(status, 0, sizeof(*status));
  return CAN_PORT_OK;
}

int main(void) {
  static const CanPortOps ops = {
    .configure = fake_configure,
    .start = fake_start,
    .send = fake_send,
    .receive = fake_receive,
    .get_status = fake_status,
  };

  FakeCan fake = {
    .rx_frame = {.id = 0x123, .ide = CAN_ID_STANDARD, .dlc = 2, .data = {0x12, 0x34}},
    .has_rx = true,
  };
  CanPort port;
  can_port_bind(&port, &fake, &ops);

  const CanPortConfig config = {
    .nominal_bitrate = 500000u,
    .data_bitrate = 2000000u,
    .fd_enabled = true,
    .brs_enabled = true,
    .internal_loopback = true,
  };

  ASSERT_TRUE(can_port_configure(&port, &config) == CAN_PORT_OK);
  ASSERT_TRUE(can_port_start(&port) == CAN_PORT_OK);
  ASSERT_TRUE(can_port_send(&port, &fake.rx_frame) == CAN_PORT_OK);

  CanFrame received;
  ASSERT_TRUE(can_port_receive(&port, &received) == CAN_PORT_OK);
  ASSERT_TRUE(received.id == 0x123u);
  ASSERT_TRUE(received.data[1] == 0x34u);
  ASSERT_TRUE(can_port_receive(&port, &received) == CAN_PORT_RX_EMPTY);

  CanPortStatus status;
  ASSERT_TRUE(can_port_get_status(&port, &status) == CAN_PORT_OK);
  ASSERT_TRUE(status.tx_count == 1u);
  ASSERT_TRUE(status.rx_count == 1u);
  ASSERT_TRUE(status.error_count == 0u);
  return 0;
}
