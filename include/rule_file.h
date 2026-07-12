#ifndef RULE_FILE_H
#define RULE_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rule_config.h"

#define RULE_FILE_PATH "/config/rule.conf"
#define RULE_FILE_V2_PATH "/config/rules-v2.conf"
#define RULE_FILE_V1_MAX_BYTES 256u
#define RULE_FILE_V2_MAX_BYTES 512u
#define RULE_FILE_V2_RULE_COUNT 2u

bool rule_file_parse_v1(const uint8_t *data, size_t len, RuleTaskConfig *out_config);
bool rule_file_parse_v2(const uint8_t *data, size_t len, RuleEngine *out_engine);

#endif
