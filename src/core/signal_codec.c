#include "signal_codec.h"

#include <string.h>

static bool validate_spec(const SignalSpec *spec, size_t data_len) {
  if (spec == NULL || data_len == 0u || data_len > 64u) {
    return false;
  }
  if (spec->bit_length == 0u || spec->bit_length > 63u) {
    return false;
  }

  const uint16_t frame_bits = (uint16_t)(data_len * 8u);
  if (spec->start_bit >= frame_bits) {
    return false;
  }

  if (spec->byte_order == SIGNAL_ENDIAN_INTEL) {
    return (uint32_t)spec->start_bit + spec->bit_length <= frame_bits;
  }

  int bit = (int)spec->start_bit;
  for (uint8_t i = 0u; i < spec->bit_length; ++i) {
    if (bit < 0 || bit >= frame_bits) {
      return false;
    }
    bit = (bit % 8 == 0) ? bit + 15 : bit - 1;
  }
  return true;
}

static uint8_t get_bit(const uint8_t *data, uint16_t bit_index) {
  return (uint8_t)((data[bit_index / 8u] >> (bit_index % 8u)) & 1u);
}

static void set_bit(uint8_t *data, uint16_t bit_index, uint8_t value) {
  const uint8_t mask = (uint8_t)(1u << (bit_index % 8u));
  if (value != 0u) {
    data[bit_index / 8u] = (uint8_t)(data[bit_index / 8u] | mask);
  } else {
    data[bit_index / 8u] = (uint8_t)(data[bit_index / 8u] & (uint8_t)~mask);
  }
}

static int next_motorola_bit(int bit) {
  return (bit % 8 == 0) ? bit + 15 : bit - 1;
}

static int64_t sign_extend(uint64_t raw, uint8_t bit_length) {
  if (bit_length >= 63u) {
    return (int64_t)raw;
  }

  const uint64_t sign_bit = 1ull << (bit_length - 1u);
  if ((raw & sign_bit) == 0u) {
    return (int64_t)raw;
  }

  const uint64_t mask = ~((1ull << bit_length) - 1ull);
  return (int64_t)(raw | mask);
}

bool signal_extract_raw(const uint8_t *data, size_t data_len, const SignalSpec *spec, int64_t *raw_value) {
  if (data == NULL || raw_value == NULL || !validate_spec(spec, data_len)) {
    return false;
  }

  uint64_t raw = 0u;
  if (spec->byte_order == SIGNAL_ENDIAN_INTEL) {
    for (uint8_t i = 0u; i < spec->bit_length; ++i) {
      raw |= (uint64_t)get_bit(data, (uint16_t)(spec->start_bit + i)) << i;
    }
  } else {
    int bit = (int)spec->start_bit;
    for (uint8_t i = 0u; i < spec->bit_length; ++i) {
      raw = (raw << 1u) | get_bit(data, (uint16_t)bit);
      bit = next_motorola_bit(bit);
    }
  }

  *raw_value = spec->is_signed ? sign_extend(raw, spec->bit_length) : (int64_t)raw;
  return true;
}

bool signal_insert_raw(uint8_t *data, size_t data_len, const SignalSpec *spec, int64_t raw_value) {
  if (data == NULL || !validate_spec(spec, data_len)) {
    return false;
  }

  const uint64_t mask = (1ull << spec->bit_length) - 1ull;
  const uint64_t raw = (uint64_t)raw_value & mask;

  if (spec->byte_order == SIGNAL_ENDIAN_INTEL) {
    for (uint8_t i = 0u; i < spec->bit_length; ++i) {
      set_bit(data, (uint16_t)(spec->start_bit + i), (uint8_t)((raw >> i) & 1u));
    }
  } else {
    int bit = (int)spec->start_bit;
    for (uint8_t i = 0u; i < spec->bit_length; ++i) {
      const uint8_t shift = (uint8_t)(spec->bit_length - 1u - i);
      set_bit(data, (uint16_t)bit, (uint8_t)((raw >> shift) & 1u));
      bit = next_motorola_bit(bit);
    }
  }

  return true;
}

bool signal_decode_phys(const uint8_t *data, size_t data_len, const SignalSpec *spec, double *phys_value) {
  int64_t raw = 0;
  if (phys_value == NULL || !signal_extract_raw(data, data_len, spec, &raw)) {
    return false;
  }

  *phys_value = (double)raw * spec->factor + spec->offset;
  return true;
}

bool signal_encode_phys(uint8_t *data, size_t data_len, const SignalSpec *spec, double phys_value) {
  if (spec == NULL || spec->factor == 0.0) {
    return false;
  }

  const double scaled = (phys_value - spec->offset) / spec->factor;
  const int64_t raw = (int64_t)(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
  return signal_insert_raw(data, data_len, spec, raw);
}
