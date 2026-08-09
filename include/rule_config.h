#ifndef RULE_CONFIG_H
#define RULE_CONFIG_H

#include "rule_engine.h"

typedef struct {
  double on_threshold;
  double off_threshold;
  uint32_t delay_ms;
  uint32_t timeout_ms;
} RuleTaskConfig;

bool rule_task_config_load(RuleEngine *engine, const RuleTaskConfig *config);

#endif
