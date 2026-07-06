#ifndef W5500_PORT_H
#define W5500_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  W5500_OK = 0,
  W5500_ERROR,
  W5500_VERSION_MISMATCH,
} W5500Result;

typedef struct {
  uint8_t mac[6];
  uint8_t ip[4];
  uint8_t netmask[4];
  uint8_t gateway[4];
  uint16_t retry_time_100us;
  uint8_t retry_count;
} W5500Config;

typedef struct {
  bool initialized;
  bool network_configured;
  bool link_up;
  uint8_t version;
  uint8_t phycfgr;
} W5500Status;

typedef struct {
  void (*select)(void *ctx);
  void (*deselect)(void *ctx);
  void (*reset_write)(void *ctx, bool level_high);
  void (*delay_ms)(void *ctx, uint32_t ms);
  W5500Result (*transfer)(void *ctx, uint8_t tx, uint8_t *rx);
} W5500PortOps;

typedef struct {
  void *ctx;
  const W5500PortOps *ops;
  W5500Status status;
} W5500Port;

void w5500_port_bind(W5500Port *port, void *ctx, const W5500PortOps *ops);
W5500Result w5500_port_init(W5500Port *port, const W5500Config *config);
W5500Result w5500_port_read_reg(W5500Port *port, uint16_t address, uint8_t *value);
W5500Result w5500_port_write_reg(W5500Port *port, uint16_t address, uint8_t value);
W5500Result w5500_port_get_status(W5500Port *port, W5500Status *status);

#endif
