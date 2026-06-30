#ifndef CAN_PORT_H
#define CAN_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "can_types.h"

typedef enum {
  CAN_PORT_OK = 0,
  CAN_PORT_ERROR,
  CAN_PORT_NOT_READY,
  CAN_PORT_RX_EMPTY,
} CanPortResult;

typedef struct {
  uint32_t nominal_bitrate;
  uint32_t data_bitrate;
  bool fd_enabled;
  bool brs_enabled;
  bool internal_loopback;
} CanPortConfig;

typedef struct {
  uint32_t rx_count;
  uint32_t tx_count;
  uint32_t error_count;
  bool bus_off;
  uint8_t tec;
  uint8_t rec;
} CanPortStatus;

typedef struct {
  CanPortResult (*configure)(void *ctx, const CanPortConfig *config);
  CanPortResult (*start)(void *ctx);
  CanPortResult (*send)(void *ctx, const CanFrame *frame);
  CanPortResult (*receive)(void *ctx, CanFrame *frame);
  CanPortResult (*get_status)(void *ctx, CanPortStatus *status);
} CanPortOps;

typedef struct {
  void *ctx;
  const CanPortOps *ops;
  bool started;
  CanPortStatus status;
} CanPort;

void can_port_bind(CanPort *port, void *ctx, const CanPortOps *ops);
CanPortResult can_port_configure(CanPort *port, const CanPortConfig *config);
CanPortResult can_port_start(CanPort *port);
CanPortResult can_port_send(CanPort *port, const CanFrame *frame);
CanPortResult can_port_receive(CanPort *port, CanFrame *frame);
CanPortResult can_port_get_status(CanPort *port, CanPortStatus *status);

#endif
