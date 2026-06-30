#include "ports/lan8720_port.h"

#include <string.h>

static bool has_required_ops(const Lan8720Port *port) {
  return port != NULL && port->ops != NULL && port->ops->init != NULL &&
         port->ops->start != NULL && port->ops->poll != NULL &&
         port->ops->link_up != NULL && port->ops->get_ip != NULL;
}

void lan8720_port_bind(Lan8720Port *port, void *ctx, const Lan8720PortOps *ops) {
  if (port == NULL) {
    return;
  }

  memset(port, 0, sizeof(*port));
  port->ctx = ctx;
  port->ops = ops;
}

Lan8720Result lan8720_port_init(Lan8720Port *port, const Lan8720Config *config) {
  if (!has_required_ops(port) || config == NULL) {
    return LAN8720_ERROR;
  }

  const Lan8720Result result = port->ops->init(port->ctx, config);
  port->status.initialized = result == LAN8720_OK;
  return result;
}

Lan8720Result lan8720_port_start(Lan8720Port *port) {
  if (!has_required_ops(port) || !port->status.initialized) {
    return LAN8720_ERROR;
  }

  const Lan8720Result result = port->ops->start(port->ctx);
  port->status.link_up = port->ops->link_up(port->ctx);
  port->status.ip = port->ops->get_ip(port->ctx);
  return result;
}

Lan8720Result lan8720_port_poll(Lan8720Port *port) {
  if (!has_required_ops(port) || !port->status.initialized) {
    return LAN8720_ERROR;
  }

  const Lan8720Result result = port->ops->poll(port->ctx);
  if (result != LAN8720_OK) {
    return result;
  }

  ++port->status.poll_count;
  port->status.link_up = port->ops->link_up(port->ctx);
  port->status.ip = port->ops->get_ip(port->ctx);
  return port->status.link_up ? LAN8720_OK : LAN8720_LINK_DOWN;
}

Lan8720Result lan8720_port_get_status(Lan8720Port *port, Lan8720Status *status) {
  if (!has_required_ops(port) || status == NULL) {
    return LAN8720_ERROR;
  }

  port->status.link_up = port->ops->link_up(port->ctx);
  port->status.ip = port->ops->get_ip(port->ctx);
  *status = port->status;
  return LAN8720_OK;
}
