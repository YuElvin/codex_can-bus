#ifndef TF_CARD_PORT_H
#define TF_CARD_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  TF_CARD_OK = 0,
  TF_CARD_ERROR,
  TF_CARD_NO_CARD,
  TF_CARD_VERIFY_FAILED,
} TfCardResult;

typedef struct {
  bool present;
  bool mounted;
  bool dirs_created;
  bool file_written;
  bool file_verified;
} TfCardSmokeResult;

typedef struct {
  bool (*card_present)(void *ctx);
  TfCardResult (*mount)(void *ctx);
  TfCardResult (*mkdir)(void *ctx, const char *path);
  TfCardResult (*write_file)(void *ctx, const char *path, const uint8_t *data, size_t len);
  TfCardResult (*read_file)(void *ctx, const char *path, uint8_t *data, size_t len, size_t *read_len);
} TfCardPortOps;

typedef struct {
  void *ctx;
  const TfCardPortOps *ops;
  bool mounted;
} TfCardPort;

void tf_card_port_bind(TfCardPort *port, void *ctx, const TfCardPortOps *ops);
TfCardResult tf_card_mount(TfCardPort *port);
TfCardResult tf_card_run_smoke_test(TfCardPort *port, TfCardSmokeResult *result);

#endif
