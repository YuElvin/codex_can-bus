#include "rule_config.h"

bool rule_task_config_load(RuleEngine *engine, const RuleTaskConfig *config) {
  if (engine == NULL || config == NULL || config->on_threshold <= config->off_threshold ||
      config->delay_ms > config->timeout_ms) {
    return false;
  }

  const Rule rule = {
    .id = "can2_marker",
    .enabled = true,
    .signal_key = "Can2Data.marker",
    .op = RULE_OP_HYSTERESIS_HIGH,
    .on_threshold = config->on_threshold,
    .off_threshold = config->off_threshold,
    .relay = 0u,
    .action_state = RELAY_STATE_ON,
    .delay_ms = config->delay_ms,
    .timeout_ms = config->timeout_ms,
    .safe_state = RELAY_STATE_OFF,
    .default_state = RELAY_STATE_OFF,
  };

  rule_engine_init(engine);
  return rule_engine_add_rule(engine, &rule);
}
