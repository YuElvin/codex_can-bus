#include "ports/can_port.h"

#include <string.h>

static bool has_required_ops(const CanPort *port) {
  return port != NULL && port->ops != NULL && port->ops->configure != NULL &&
         port->ops->start != NULL && port->ops->send != NULL && port->ops->receive != NULL &&
         port->ops->get_status != NULL;
}

void can_port_bind(CanPort *port, void *ctx, const CanPortOps *ops) {
  if (port == NULL) {
    return;
  }

  memset(port, 0, sizeof(*port));
  port->ctx = ctx;
  port->ops = ops;
}

CanPortResult can_port_configure(CanPort *port, const CanPortConfig *config) {
  if (!has_required_ops(port) || config == NULL) {
    return CAN_PORT_ERROR;
  }
  return port->ops->configure(port->ctx, config);
}

CanPortResult can_port_start(CanPort *port) {
  if (!has_required_ops(port)) {
    return CAN_PORT_ERROR;
  }

  const CanPortResult result = port->ops->start(port->ctx);
  port->started = result == CAN_PORT_OK;
  return result;
}

CanPortResult can_port_send(CanPort *port, const CanFrame *frame) {
  if (!has_required_ops(port) || frame == NULL) {
    return CAN_PORT_ERROR;
  }
  if (!port->started) {
    return CAN_PORT_NOT_READY;
  }

  const CanPortResult result = port->ops->send(port->ctx, frame);
  if (result == CAN_PORT_OK) {
    ++port->status.tx_count;
  } else if (result != CAN_PORT_RX_EMPTY) {
    ++port->status.error_count;
  }
  return result;
}

CanPortResult can_port_receive(CanPort *port, CanFrame *frame) {
  if (!has_required_ops(port) || frame == NULL) {
    return CAN_PORT_ERROR;
  }
  if (!port->started) {
    return CAN_PORT_NOT_READY;
  }

  const CanPortResult result = port->ops->receive(port->ctx, frame);
  if (result == CAN_PORT_OK) {
    ++port->status.rx_count;
  } else if (result != CAN_PORT_RX_EMPTY) {
    ++port->status.error_count;
  }
  return result;
}

CanPortResult can_port_get_status(CanPort *port, CanPortStatus *status) {
  if (!has_required_ops(port) || status == NULL) {
    return CAN_PORT_ERROR;
  }

  CanPortStatus backend_status;
  const CanPortResult result = port->ops->get_status(port->ctx, &backend_status);
  if (result != CAN_PORT_OK) {
    return result;
  }

  backend_status.rx_count += port->status.rx_count;
  backend_status.tx_count += port->status.tx_count;
  backend_status.error_count += port->status.error_count;
  port->status = backend_status;
  *status = backend_status;
  return CAN_PORT_OK;
}
