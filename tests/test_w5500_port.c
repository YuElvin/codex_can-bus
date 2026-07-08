#include "ports/w5500_port.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define W5500_REG_GAR 0x0001u
#define W5500_REG_SUBR 0x0005u
#define W5500_REG_SHAR 0x0009u
#define W5500_REG_SIPR 0x000fu
#define W5500_REG_RCR 0x001bu
#define W5500_REG_PHYCFGR 0x002eu
#define W5500_REG_VERSIONR 0x0039u

typedef struct {
  uint8_t regs[8][512];
  uint8_t header[3];
  uint16_t address;
  uint8_t block;
  size_t header_len;
  bool selected;
  bool write;
  bool reset_high;
  uint32_t delays;
} FakeW5500;

static void fake_select(void *ctx) {
  FakeW5500 *fake = (FakeW5500 *)ctx;
  fake->selected = true;
  fake->header_len = 0u;
}

static void fake_deselect(void *ctx) {
  ((FakeW5500 *)ctx)->selected = false;
}

static void fake_reset_write(void *ctx, bool level_high) {
  ((FakeW5500 *)ctx)->reset_high = level_high;
}

static void fake_delay_ms(void *ctx, uint32_t ms) {
  ((FakeW5500 *)ctx)->delays += ms;
}

static W5500Result fake_transfer(void *ctx, uint8_t tx, uint8_t *rx) {
  FakeW5500 *fake = (FakeW5500 *)ctx;
  if (!fake->selected || rx == NULL) {
    return W5500_ERROR;
  }

  *rx = 0u;
  if (fake->header_len < sizeof(fake->header)) {
    fake->header[fake->header_len++] = tx;
    if (fake->header_len == sizeof(fake->header)) {
      fake->address = (uint16_t)(((uint16_t)fake->header[0] << 8) | fake->header[1]);
      fake->block = (uint8_t)(fake->header[2] >> 3);
      fake->write = (fake->header[2] & 0x04u) != 0u;
    }
    return W5500_OK;
  }

  if (fake->block >= 8u || fake->address >= 512u) {
    return W5500_ERROR;
  }
  if (fake->write) {
    fake->regs[fake->block][fake->address++] = tx;
  } else {
    *rx = fake->regs[fake->block][fake->address++];
  }
  return W5500_OK;
}

static const W5500PortOps fake_ops = {
  .select = fake_select,
  .deselect = fake_deselect,
  .reset_write = fake_reset_write,
  .delay_ms = fake_delay_ms,
  .transfer = fake_transfer,
};

static int init_writes_and_verifies_network_registers(void) {
  FakeW5500 fake = {0};
  fake.regs[0][W5500_REG_VERSIONR] = 0x04u;
  fake.regs[0][W5500_REG_PHYCFGR] = 0x01u;

  W5500Port port;
  w5500_port_bind(&port, &fake, &fake_ops);

  const W5500Config config = {
    .mac = {0x02u, 0x00u, 0x00u, 0x12u, 0x34u, 0x56u},
    .ip = {192u, 168u, 1u, 88u},
    .netmask = {255u, 255u, 255u, 0u},
    .gateway = {192u, 168u, 1u, 1u},
    .retry_time_100us = 2000u,
    .retry_count = 8u,
  };

  ASSERT_TRUE(w5500_port_init(&port, &config) == W5500_OK);
  ASSERT_TRUE(fake.reset_high);
  ASSERT_TRUE(fake.delays >= 57u);
  ASSERT_TRUE(memcmp(&fake.regs[0][W5500_REG_GAR], config.gateway, sizeof(config.gateway)) == 0);
  ASSERT_TRUE(memcmp(&fake.regs[0][W5500_REG_SUBR], config.netmask, sizeof(config.netmask)) == 0);
  ASSERT_TRUE(memcmp(&fake.regs[0][W5500_REG_SHAR], config.mac, sizeof(config.mac)) == 0);
  ASSERT_TRUE(memcmp(&fake.regs[0][W5500_REG_SIPR], config.ip, sizeof(config.ip)) == 0);
  ASSERT_TRUE(fake.regs[0][W5500_REG_RCR] == config.retry_count);

  W5500Status status;
  ASSERT_TRUE(w5500_port_get_status(&port, &status) == W5500_OK);
  ASSERT_TRUE(status.initialized);
  ASSERT_TRUE(status.network_configured);
  ASSERT_TRUE(status.version == 0x04u);
  ASSERT_TRUE(status.link_up);
  return 0;
}

static int rejects_missing_or_wrong_version(void) {
  FakeW5500 fake = {0};
  W5500Port port;
  w5500_port_bind(&port, &fake, &fake_ops);

  const W5500Config config = {
    .mac = {0x02u, 0x00u, 0x00u, 0x12u, 0x34u, 0x56u},
    .ip = {192u, 168u, 1u, 88u},
    .netmask = {255u, 255u, 255u, 0u},
    .gateway = {192u, 168u, 1u, 1u},
  };

  ASSERT_TRUE(w5500_port_init(&port, &config) == W5500_VERSION_MISMATCH);
  ASSERT_TRUE(port.status.version == 0x00u);
  return 0;
}

static int supports_non_common_blocks(void) {
  FakeW5500 fake = {0};
  W5500Port port;
  w5500_port_bind(&port, &fake, &fake_ops);

  const uint8_t tx_data[3] = {0xaau, 0xbbu, 0xccu};
  uint8_t rx_data[3] = {0};
  ASSERT_TRUE(w5500_port_write_block(&port, 2u, 0x0024u, tx_data, sizeof(tx_data)) == W5500_OK);
  ASSERT_TRUE(memcmp(&fake.regs[2][0x0024u], tx_data, sizeof(tx_data)) == 0);
  ASSERT_TRUE(w5500_port_read_block(&port, 2u, 0x0024u, rx_data, sizeof(rx_data)) == W5500_OK);
  ASSERT_TRUE(memcmp(rx_data, tx_data, sizeof(rx_data)) == 0);
  ASSERT_TRUE(fake.header[2] == (uint8_t)(2u << 3));
  return 0;
}

int main(void) {
  if (init_writes_and_verifies_network_registers() != 0) {
    return 1;
  }
  if (rejects_missing_or_wrong_version() != 0) {
    return 1;
  }
  if (supports_non_common_blocks() != 0) {
    return 1;
  }
  return 0;
}
