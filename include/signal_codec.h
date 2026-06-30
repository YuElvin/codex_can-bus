#ifndef SIGNAL_CODEC_H
#define SIGNAL_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  SIGNAL_ENDIAN_MOTOROLA = 0,
  SIGNAL_ENDIAN_INTEL = 1,
} SignalEndian;

typedef struct {
  uint16_t start_bit;
  uint8_t bit_length;
  SignalEndian byte_order;
  bool is_signed;
  double factor;
  double offset;
  double minimum;
  double maximum;
} SignalSpec;

bool signal_extract_raw(const uint8_t *data, size_t data_len, const SignalSpec *spec, int64_t *raw_value);
bool signal_insert_raw(uint8_t *data, size_t data_len, const SignalSpec *spec, int64_t raw_value);
bool signal_decode_phys(const uint8_t *data, size_t data_len, const SignalSpec *spec, double *phys_value);
bool signal_encode_phys(uint8_t *data, size_t data_len, const SignalSpec *spec, double phys_value);

#endif
