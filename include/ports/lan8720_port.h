#ifndef LAN8720_PORT_H
#define LAN8720_PORT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  LAN8720_OK = 0,
  LAN8720_ERROR,
  LAN8720_LINK_DOWN,
} Lan8720Result;

typedef struct {
  uint8_t mac[6];
  uint32_t ip;
  uint32_t netmask;
  uint32_t gateway;
  bool dhcp_enabled;
} Lan8720Config;

typedef struct {
  bool initialized;
  bool link_up;
  uint32_t ip;
  uint32_t poll_count;
} Lan8720Status;

typedef struct {
  Lan8720Result (*init)(void *ctx, const Lan8720Config *config);
  Lan8720Result (*start)(void *ctx);
  Lan8720Result (*poll)(void *ctx);
  bool (*link_up)(void *ctx);
  uint32_t (*get_ip)(void *ctx);
} Lan8720PortOps;

typedef struct {
  void *ctx;
  const Lan8720PortOps *ops;
  Lan8720Status status;
} Lan8720Port;

void lan8720_port_bind(Lan8720Port *port, void *ctx, const Lan8720PortOps *ops);
Lan8720Result lan8720_port_init(Lan8720Port *port, const Lan8720Config *config);
Lan8720Result lan8720_port_start(Lan8720Port *port);
Lan8720Result lan8720_port_poll(Lan8720Port *port);
Lan8720Result lan8720_port_get_status(Lan8720Port *port, Lan8720Status *status);

#endif
