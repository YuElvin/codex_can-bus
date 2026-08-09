#include "can_tx_control.h"

#include <limits.h>
#include <string.h>

enum {
  CAN_TX_FIELD_ENABLED = 1u << 0,
  CAN_TX_FIELD_ID = 1u << 1,
  CAN_TX_FIELD_DLC = 1u << 2,
  CAN_TX_FIELD_DATA = 1u << 3,
  CAN_TX_FIELD_PERIOD_MS = 1u << 4,
  CAN_TX_FIELD_ALL = CAN_TX_FIELD_ENABLED | CAN_TX_FIELD_ID | CAN_TX_FIELD_DLC |
                     CAN_TX_FIELD_DATA | CAN_TX_FIELD_PERIOD_MS,
};

static bool span_is(const char *text, size_t len, const char *expected) {
  const size_t expected_len = strlen(expected);
  return len == expected_len && memcmp(text, expected, len) == 0;
}

static bool parse_u32(const char *text, size_t len, uint32_t *value) {
  uint32_t result = 0u;

  if (text == NULL || value == NULL || len == 0u) return false;
  for (size_t i = 0u; i < len; ++i) {
    const uint8_t ch = (uint8_t)text[i];
    if (ch < (uint8_t)'0' || ch > (uint8_t)'9' ||
        result > (UINT32_MAX - (uint32_t)(ch - (uint8_t)'0')) / 10u) {
      return false;
    }
    result = (result * 10u) + (uint32_t)(ch - (uint8_t)'0');
  }
  *value = result;
  return true;
}

static bool hex_nibble(char ch, uint8_t *value) {
  if (ch >= '0' && ch <= '9') {
    *value = (uint8_t)(ch - '0');
    return true;
  }
  if (ch >= 'a' && ch <= 'f') {
    *value = (uint8_t)(ch - 'a' + 10);
    return true;
  }
  if (ch >= 'A' && ch <= 'F') {
    *value = (uint8_t)(ch - 'A' + 10);
    return true;
  }
  return false;
}

static bool parse_data(const char *text, size_t len, uint8_t data[8]) {
  if (len != 16u) return false;
  for (size_t i = 0u; i < 8u; ++i) {
    uint8_t high;
    uint8_t low;
    if (!hex_nibble(text[i * 2u], &high) || !hex_nibble(text[i * 2u + 1u], &low)) return false;
    data[i] = (uint8_t)((high << 4u) | low);
  }
  return true;
}

void can_tx_control_init(CanTxControlState *state) {
  static const uint8_t default_data[8] = {0xc2u, 0xa5u, 0x00u, 0x01u,
                                          0x02u, 0x03u, 0x04u, 0x05u};

  if (state == NULL) return;
  memset(state, 0, sizeof(*state));
  state->config.enabled = true;
  state->config.standard_id = 0x321u;
  state->config.dlc = 8u;
  memcpy(state->config.data, default_data, sizeof(default_data));
  state->config.period_ms = 1000u;
}

bool can_tx_control_parse_form(const char *body, size_t body_len, CanTxControlConfig *config) {
  uint32_t fields = 0u;
  size_t offset = 0u;
  CanTxControlConfig candidate;

  if (body == NULL || config == NULL || body_len == 0u || body[body_len - 1u] == '&') return false;
  memset(&candidate, 0, sizeof(candidate));
  while (offset < body_len) {
    size_t equals = offset;
    size_t entry_end;
    uint32_t value;

    while (equals < body_len && body[equals] != '=' && body[equals] != '&') ++equals;
    if (equals == offset || equals == body_len || body[equals] != '=') return false;
    entry_end = equals + 1u;
    while (entry_end < body_len && body[entry_end] != '&') ++entry_end;
    if (entry_end == equals + 1u) return false;

    const char *key = &body[offset];
    const size_t key_len = equals - offset;
    const char *text = &body[equals + 1u];
    const size_t text_len = entry_end - equals - 1u;
    if (span_is(key, key_len, "enabled")) {
      if ((fields & CAN_TX_FIELD_ENABLED) != 0u || text_len != 1u ||
          (text[0] != '0' && text[0] != '1')) return false;
      candidate.enabled = text[0] == '1';
      fields |= CAN_TX_FIELD_ENABLED;
    } else if (span_is(key, key_len, "id")) {
      if ((fields & CAN_TX_FIELD_ID) != 0u || !parse_u32(text, text_len, &value) || value > 0x7ffu) {
        return false;
      }
      candidate.standard_id = (uint16_t)value;
      fields |= CAN_TX_FIELD_ID;
    } else if (span_is(key, key_len, "dlc")) {
      if ((fields & CAN_TX_FIELD_DLC) != 0u || !parse_u32(text, text_len, &value) || value > 8u) {
        return false;
      }
      candidate.dlc = (uint8_t)value;
      fields |= CAN_TX_FIELD_DLC;
    } else if (span_is(key, key_len, "data")) {
      if ((fields & CAN_TX_FIELD_DATA) != 0u || !parse_data(text, text_len, candidate.data)) return false;
      fields |= CAN_TX_FIELD_DATA;
    } else if (span_is(key, key_len, "periodMs")) {
      if ((fields & CAN_TX_FIELD_PERIOD_MS) != 0u || !parse_u32(text, text_len, &value) ||
          value < 100u || value > 10000u) return false;
      candidate.period_ms = value;
      fields |= CAN_TX_FIELD_PERIOD_MS;
    } else {
      return false;
    }
    offset = entry_end + (entry_end < body_len ? 1u : 0u);
  }
  if (fields != CAN_TX_FIELD_ALL) return false;
  for (size_t i = candidate.dlc; i < sizeof(candidate.data); ++i) candidate.data[i] = 0u;
  *config = candidate;
  return true;
}

void can_tx_control_submit(CanTxControlState *state,
                           const CanTxControlConfig *config,
                           uint32_t *request_seq) {
  if (state == NULL || config == NULL) return;
  state->config = *config;
  ++state->request_seq;
  state->last_result = UINT32_MAX;
  if (request_seq != NULL) *request_seq = state->request_seq;
}
