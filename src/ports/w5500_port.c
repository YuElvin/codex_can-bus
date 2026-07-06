#include "ports/w5500_port.h"

#include <string.h>

#define W5500_BLOCK_COMMON 0x00u
#define W5500_CONTROL_READ 0x00u
#define W5500_CONTROL_WRITE 0x04u

#define W5500_REG_MR 0x0000u
#define W5500_REG_GAR 0x0001u
#define W5500_REG_SUBR 0x0005u
#define W5500_REG_SHAR 0x0009u
#define W5500_REG_SIPR 0x000fu
#define W5500_REG_RTR 0x0019u
#define W5500_REG_RCR 0x001bu
#define W5500_REG_PHYCFGR 0x002eu
#define W5500_REG_VERSIONR 0x0039u

#define W5500_MR_RST 0x80u
#define W5500_EXPECTED_VERSION 0x04u
#define W5500_PHYCFGR_LINK 0x01u

static bool has_required_ops(const W5500Port *port) {
  return port != NULL && port->ops != NULL && port->ops->select != NULL &&
         port->ops->deselect != NULL && port->ops->reset_write != NULL &&
         port->ops->delay_ms != NULL && port->ops->transfer != NULL;
}

static uint8_t control_byte(uint8_t control) {
  return (uint8_t)((W5500_BLOCK_COMMON << 3) | control);
}

static W5500Result transfer_byte(W5500Port *port, uint8_t tx, uint8_t *rx) {
  uint8_t ignored = 0u;
  return port->ops->transfer(port->ctx, tx, rx == NULL ? &ignored : rx);
}

static W5500Result read_buffer(W5500Port *port, uint16_t address, uint8_t *data, size_t len) {
  if (!has_required_ops(port) || data == NULL) {
    return W5500_ERROR;
  }

  port->ops->select(port->ctx);
  W5500Result result = transfer_byte(port, (uint8_t)(address >> 8), NULL);
  if (result == W5500_OK) {
    result = transfer_byte(port, (uint8_t)address, NULL);
  }
  if (result == W5500_OK) {
    result = transfer_byte(port, control_byte(W5500_CONTROL_READ), NULL);
  }
  for (size_t i = 0u; result == W5500_OK && i < len; ++i) {
    result = transfer_byte(port, 0x00u, &data[i]);
  }
  port->ops->deselect(port->ctx);
  return result;
}

static W5500Result write_buffer(W5500Port *port, uint16_t address, const uint8_t *data, size_t len) {
  if (!has_required_ops(port) || data == NULL) {
    return W5500_ERROR;
  }

  port->ops->select(port->ctx);
  W5500Result result = transfer_byte(port, (uint8_t)(address >> 8), NULL);
  if (result == W5500_OK) {
    result = transfer_byte(port, (uint8_t)address, NULL);
  }
  if (result == W5500_OK) {
    result = transfer_byte(port, control_byte(W5500_CONTROL_WRITE), NULL);
  }
  for (size_t i = 0u; result == W5500_OK && i < len; ++i) {
    result = transfer_byte(port, data[i], NULL);
  }
  port->ops->deselect(port->ctx);
  return result;
}

static W5500Result write_u16(W5500Port *port, uint16_t address, uint16_t value) {
  const uint8_t data[2] = {
    (uint8_t)(value >> 8),
    (uint8_t)value,
  };
  return write_buffer(port, address, data, sizeof(data));
}

static bool same_bytes(W5500Port *port, uint16_t address, const uint8_t *expected, size_t len) {
  uint8_t actual[6] = {0};
  return len <= sizeof(actual) && read_buffer(port, address, actual, len) == W5500_OK &&
         memcmp(actual, expected, len) == 0;
}

static void update_status(W5500Port *port) {
  uint8_t value = 0xffu;
  if (w5500_port_read_reg(port, W5500_REG_VERSIONR, &value) == W5500_OK) {
    port->status.version = value;
  }
  if (w5500_port_read_reg(port, W5500_REG_PHYCFGR, &value) == W5500_OK) {
    port->status.phycfgr = value;
    port->status.link_up = (value & W5500_PHYCFGR_LINK) != 0u;
  }
}

void w5500_port_bind(W5500Port *port, void *ctx, const W5500PortOps *ops) {
  if (port == NULL) {
    return;
  }

  memset(port, 0, sizeof(*port));
  port->ctx = ctx;
  port->ops = ops;
  port->status.version = 0xffu;
  port->status.phycfgr = 0xffu;
}

W5500Result w5500_port_read_reg(W5500Port *port, uint16_t address, uint8_t *value) {
  if (value == NULL) {
    return W5500_ERROR;
  }
  return read_buffer(port, address, value, 1u);
}

W5500Result w5500_port_write_reg(W5500Port *port, uint16_t address, uint8_t value) {
  return write_buffer(port, address, &value, 1u);
}

W5500Result w5500_port_init(W5500Port *port, const W5500Config *config) {
  if (!has_required_ops(port) || config == NULL) {
    return W5500_ERROR;
  }

  port->ops->deselect(port->ctx);
  port->ops->reset_write(port->ctx, false);
  port->ops->delay_ms(port->ctx, 2u);
  port->ops->reset_write(port->ctx, true);
  port->ops->delay_ms(port->ctx, 50u);

  if (w5500_port_write_reg(port, W5500_REG_MR, W5500_MR_RST) != W5500_OK) {
    return W5500_ERROR;
  }
  port->ops->delay_ms(port->ctx, 5u);

  update_status(port);
  if (port->status.version != W5500_EXPECTED_VERSION) {
    return W5500_VERSION_MISMATCH;
  }

  const uint16_t retry_time = config->retry_time_100us == 0u ? 2000u : config->retry_time_100us;
  const uint8_t retry_count = config->retry_count == 0u ? 8u : config->retry_count;
  if (write_buffer(port, W5500_REG_GAR, config->gateway, sizeof(config->gateway)) != W5500_OK ||
      write_buffer(port, W5500_REG_SUBR, config->netmask, sizeof(config->netmask)) != W5500_OK ||
      write_buffer(port, W5500_REG_SHAR, config->mac, sizeof(config->mac)) != W5500_OK ||
      write_buffer(port, W5500_REG_SIPR, config->ip, sizeof(config->ip)) != W5500_OK ||
      write_u16(port, W5500_REG_RTR, retry_time) != W5500_OK ||
      w5500_port_write_reg(port, W5500_REG_RCR, retry_count) != W5500_OK) {
    return W5500_ERROR;
  }

  if (!same_bytes(port, W5500_REG_GAR, config->gateway, sizeof(config->gateway)) ||
      !same_bytes(port, W5500_REG_SUBR, config->netmask, sizeof(config->netmask)) ||
      !same_bytes(port, W5500_REG_SHAR, config->mac, sizeof(config->mac)) ||
      !same_bytes(port, W5500_REG_SIPR, config->ip, sizeof(config->ip))) {
    return W5500_ERROR;
  }

  port->status.initialized = true;
  port->status.network_configured = true;
  update_status(port);
  return W5500_OK;
}

W5500Result w5500_port_get_status(W5500Port *port, W5500Status *status) {
  if (!has_required_ops(port) || status == NULL) {
    return W5500_ERROR;
  }

  update_status(port);
  *status = port->status;
  return W5500_OK;
}
