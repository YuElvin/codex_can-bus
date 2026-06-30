#include "ports/lan8720_port.h"

#include <stdio.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

typedef struct {
  bool initialized;
  bool started;
  bool link_up;
  uint32_t ip;
  uint32_t polls;
} FakeLan;

static Lan8720Result fake_init(void *ctx, const Lan8720Config *config) {
  FakeLan *fake = (FakeLan *)ctx;
  fake->initialized = config->mac[0] == 0x02u;
  fake->ip = config->ip;
  return fake->initialized ? LAN8720_OK : LAN8720_ERROR;
}

static Lan8720Result fake_start(void *ctx) {
  FakeLan *fake = (FakeLan *)ctx;
  fake->started = fake->initialized;
  return fake->started ? LAN8720_OK : LAN8720_ERROR;
}

static Lan8720Result fake_poll(void *ctx) {
  FakeLan *fake = (FakeLan *)ctx;
  ++fake->polls;
  return LAN8720_OK;
}

static bool fake_link_up(void *ctx) {
  return ((FakeLan *)ctx)->link_up;
}

static uint32_t fake_get_ip(void *ctx) {
  return ((FakeLan *)ctx)->ip;
}

int main(void) {
  static const Lan8720PortOps ops = {
    .init = fake_init,
    .start = fake_start,
    .poll = fake_poll,
    .link_up = fake_link_up,
    .get_ip = fake_get_ip,
  };

  FakeLan fake = {.link_up = true};
  Lan8720Port port;
  lan8720_port_bind(&port, &fake, &ops);

  const Lan8720Config config = {
    .mac = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56},
    .ip = 0xc0a80158u,
    .netmask = 0xffffff00u,
    .gateway = 0xc0a80101u,
    .dhcp_enabled = false,
  };

  ASSERT_TRUE(lan8720_port_init(&port, &config) == LAN8720_OK);
  ASSERT_TRUE(lan8720_port_start(&port) == LAN8720_OK);
  ASSERT_TRUE(lan8720_port_poll(&port) == LAN8720_OK);

  Lan8720Status status;
  ASSERT_TRUE(lan8720_port_get_status(&port, &status) == LAN8720_OK);
  ASSERT_TRUE(status.initialized);
  ASSERT_TRUE(status.link_up);
  ASSERT_TRUE(status.ip == config.ip);
  ASSERT_TRUE(status.poll_count == 1u);
  return 0;
}
