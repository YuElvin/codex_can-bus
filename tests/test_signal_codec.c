#include "signal_codec.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_EQ_I64(expected, actual)                                                      \
  do {                                                                                       \
    if ((expected) != (actual)) {                                                            \
      printf("ASSERT_EQ_I64 failed at %s:%d: expected %lld got %lld\n",                      \
             __FILE__,                                                                       \
             __LINE__,                                                                       \
             (long long)(expected),                                                          \
             (long long)(actual));                                                           \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_EQ_U8(expected, actual)                                                       \
  do {                                                                                       \
    if ((expected) != (actual)) {                                                            \
      printf("ASSERT_EQ_U8 failed at %s:%d: expected 0x%02x got 0x%02x\n",                   \
             __FILE__,                                                                       \
             __LINE__,                                                                       \
             (unsigned int)(expected),                                                       \
             (unsigned int)(actual));                                                        \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

static int test_intel_unsigned_decode_encode(void) {
  uint8_t data[8] = {0x34, 0x12};
  const SignalSpec spec = {
    .start_bit = 0,
    .bit_length = 16,
    .byte_order = SIGNAL_ENDIAN_INTEL,
    .is_signed = false,
    .factor = 0.5,
    .offset = 0.0,
  };

  int64_t raw = 0;
  double phys = 0.0;
  ASSERT_TRUE(signal_extract_raw(data, sizeof(data), &spec, &raw));
  ASSERT_EQ_I64(0x1234, raw);
  ASSERT_TRUE(signal_decode_phys(data, sizeof(data), &spec, &phys));
  ASSERT_TRUE(phys == 2330.0);

  memset(data, 0, sizeof(data));
  ASSERT_TRUE(signal_encode_phys(data, sizeof(data), &spec, 2330.0));
  ASSERT_EQ_U8(0x34, data[0]);
  ASSERT_EQ_U8(0x12, data[1]);
  return 0;
}

static int test_signed_decode(void) {
  const uint8_t data[1] = {0xff};
  const SignalSpec spec = {
    .start_bit = 0,
    .bit_length = 8,
    .byte_order = SIGNAL_ENDIAN_INTEL,
    .is_signed = true,
    .factor = 1.0,
    .offset = 0.0,
  };

  int64_t raw = 0;
  ASSERT_TRUE(signal_extract_raw(data, sizeof(data), &spec, &raw));
  ASSERT_EQ_I64(-1, raw);
  return 0;
}

static int test_motorola_byte_aligned_decode_encode(void) {
  uint8_t data[8] = {0x12, 0x34};
  const SignalSpec spec = {
    .start_bit = 7,
    .bit_length = 16,
    .byte_order = SIGNAL_ENDIAN_MOTOROLA,
    .is_signed = false,
    .factor = 1.0,
    .offset = 0.0,
  };

  int64_t raw = 0;
  ASSERT_TRUE(signal_extract_raw(data, sizeof(data), &spec, &raw));
  ASSERT_EQ_I64(0x1234, raw);

  memset(data, 0, sizeof(data));
  ASSERT_TRUE(signal_insert_raw(data, sizeof(data), &spec, 0x1234));
  ASSERT_EQ_U8(0x12, data[0]);
  ASSERT_EQ_U8(0x34, data[1]);
  return 0;
}

int main(void) {
  ASSERT_TRUE(test_intel_unsigned_decode_encode() == 0);
  ASSERT_TRUE(test_signed_decode() == 0);
  ASSERT_TRUE(test_motorola_byte_aligned_decode_encode() == 0);
  return 0;
}
