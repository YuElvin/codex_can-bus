#include "ports/tf_card_port.h"

#include <string.h>

static const char *const required_dirs[] = {
  "/www",
  "/dbc",
  "/log",
  "/config",
  "/sys",
};

static bool has_required_ops(const TfCardPort *port) {
  return port != NULL && port->ops != NULL && port->ops->card_present != NULL &&
         port->ops->mount != NULL && port->ops->mkdir != NULL &&
         port->ops->write_file != NULL && port->ops->read_file != NULL;
}

void tf_card_port_bind(TfCardPort *port, void *ctx, const TfCardPortOps *ops) {
  if (port == NULL) {
    return;
  }

  memset(port, 0, sizeof(*port));
  port->ctx = ctx;
  port->ops = ops;
}

TfCardResult tf_card_mount(TfCardPort *port) {
  if (!has_required_ops(port)) {
    return TF_CARD_ERROR;
  }
  if (!port->ops->card_present(port->ctx)) {
    return TF_CARD_NO_CARD;
  }

  const TfCardResult result = port->ops->mount(port->ctx);
  port->mounted = result == TF_CARD_OK;
  return result;
}

TfCardResult tf_card_run_smoke_test(TfCardPort *port, TfCardSmokeResult *result) {
  static const uint8_t payload[] = "can_bus_tf_smoke\n";
  uint8_t buffer[sizeof(payload)] = {0};
  size_t read_len = 0u;

  if (!has_required_ops(port) || result == NULL) {
    return TF_CARD_ERROR;
  }

  memset(result, 0, sizeof(*result));
  result->present = port->ops->card_present(port->ctx);
  if (!result->present) {
    return TF_CARD_NO_CARD;
  }

  TfCardResult op_result = tf_card_mount(port);
  result->mounted = op_result == TF_CARD_OK;
  if (op_result != TF_CARD_OK) {
    return op_result;
  }

  for (size_t i = 0u; i < sizeof(required_dirs) / sizeof(required_dirs[0]); ++i) {
    op_result = port->ops->mkdir(port->ctx, required_dirs[i]);
    if (op_result != TF_CARD_OK) {
      return op_result;
    }
  }
  result->dirs_created = true;

  op_result = port->ops->write_file(port->ctx, "/sys/smoke.txt", payload, sizeof(payload));
  result->file_written = op_result == TF_CARD_OK;
  if (op_result != TF_CARD_OK) {
    return op_result;
  }

  op_result = port->ops->read_file(port->ctx, "/sys/smoke.txt", buffer, sizeof(buffer), &read_len);
  if (op_result != TF_CARD_OK) {
    return op_result;
  }

  result->file_verified = read_len == sizeof(payload) && memcmp(buffer, payload, sizeof(payload)) == 0;
  return result->file_verified ? TF_CARD_OK : TF_CARD_VERIFY_FAILED;
}
