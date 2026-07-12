#ifndef RULE_FILE_H
#define RULE_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rule_config.h"

#define RULE_FILE_PATH "/config/rule.conf"
#define RULE_FILE_V1_MAX_BYTES 256u

bool rule_file_parse_v1(const uint8_t *data, size_t len, RuleTaskConfig *out_config);

#endif
