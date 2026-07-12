#ifndef RULE_FILE_H
#define RULE_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rule_config.h"

#define RULE_FILE_PATH "/config/rule.conf"
#define RULE_FILE_V2_PATH "/config/rules-v2.conf"
#define RULE_FILE_V3_PATH "/config/rules-v3.conf"
#define RULE_FILE_V1_MAX_BYTES 256u
#define RULE_FILE_V2_MAX_BYTES 512u
#define RULE_FILE_V3_MAX_BYTES 640u
#define RULE_FILE_V2_RULE_COUNT 2u

typedef struct {
  bool enabled;
  uint8_t relay;
  uint32_t threshold;
  RelayState action_state;
  uint32_t delay_ms;
  uint32_t timeout_ms;
  RelayState safe_state;
  uint8_t priority;
} RuleFileV3Slot;

typedef struct {
  RuleFileV3Slot slots[RULE_FILE_V2_RULE_COUNT];
} RuleFileV3;

bool rule_file_parse_v1(const uint8_t *data, size_t len, RuleTaskConfig *out_config);
bool rule_file_parse_v2(const uint8_t *data, size_t len, RuleEngine *out_engine);
bool rule_file_parse_v3(const uint8_t *data, size_t len, RuleFileV3 *out_rules);
bool rule_file_v3_build_engine(const RuleFileV3 *rules, RuleEngine *out_engine);
size_t rule_file_format_v3(const RuleFileV3 *rules, char *out_text, size_t out_capacity);

#endif
