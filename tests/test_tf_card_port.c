#include "ports/tf_card_port.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

typedef struct {
  bool present;
  bool mounted;
  unsigned int mkdir_count;
  uint8_t file_data[64];
  size_t file_len;
} FakeTf;

static bool fake_present(void *ctx) {
  return ((FakeTf *)ctx)->present;
}

static TfCardResult fake_mount(void *ctx) {
  FakeTf *fake = (FakeTf *)ctx;
  fake->mounted = fake->present;
  return fake->mounted ? TF_CARD_OK : TF_CARD_NO_CARD;
}

static TfCardResult fake_mkdir(void *ctx, const char *path) {
  FakeTf *fake = (FakeTf *)ctx;
  ASSERT_TRUE(path[0] == '/');
  ++fake->mkdir_count;
  return TF_CARD_OK;
}

static TfCardResult fake_write(void *ctx, const char *path, const uint8_t *data, size_t len) {
  FakeTf *fake = (FakeTf *)ctx;
  ASSERT_TRUE(strcmp(path, "/sys/smoke.txt") == 0);
  ASSERT_TRUE(len <= sizeof(fake->file_data));
  memcpy(fake->file_data, data, len);
  fake->file_len = len;
  return TF_CARD_OK;
}

static TfCardResult fake_read(void *ctx, const char *path, uint8_t *data, size_t len, size_t *read_len) {
  FakeTf *fake = (FakeTf *)ctx;
  ASSERT_TRUE(strcmp(path, "/sys/smoke.txt") == 0);
  ASSERT_TRUE(len >= fake->file_len);
  memcpy(data, fake->file_data, fake->file_len);
  *read_len = fake->file_len;
  return TF_CARD_OK;
}

int main(void) {
  static const TfCardPortOps ops = {
    .card_present = fake_present,
    .mount = fake_mount,
    .mkdir = fake_mkdir,
    .write_file = fake_write,
    .read_file = fake_read,
  };

  FakeTf fake = {.present = true};
  TfCardPort port;
  tf_card_port_bind(&port, &fake, &ops);

  TfCardSmokeResult result;
  ASSERT_TRUE(tf_card_run_smoke_test(&port, &result) == TF_CARD_OK);
  ASSERT_TRUE(result.present);
  ASSERT_TRUE(result.mounted);
  ASSERT_TRUE(result.dirs_created);
  ASSERT_TRUE(result.file_written);
  ASSERT_TRUE(result.file_verified);
  ASSERT_TRUE(fake.mkdir_count == 5u);
  return 0;
}
