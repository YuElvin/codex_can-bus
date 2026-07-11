#include "rule_config.h"

#include <stdio.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\\n", __FILE__, __LINE__, #expr);              \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

int main(void) {
  const RuleTaskConfig valid = {.on_threshold = 42434.0,
                                .off_threshold = 42432.0,
                                .delay_ms = 1000u,
                                .timeout_ms = 1500u};
  RuleEngine engine;

  ASSERT_TRUE(rule_task_config_load(&engine, &valid));
  ASSERT_TRUE(engine.rule_count == 1u);
  ASSERT_TRUE(engine.rules[0].op == RULE_OP_HYSTERESIS_HIGH);
  ASSERT_TRUE(engine.rules[0].on_threshold == 42434.0);
  ASSERT_TRUE(engine.rules[0].off_threshold == 42432.0);
  ASSERT_TRUE(!rule_task_config_load(&engine,
                                     &(RuleTaskConfig){.on_threshold = 42432.0,
                                                       .off_threshold = 42434.0,
                                                       .delay_ms = 1000u,
                                                       .timeout_ms = 1500u}));
  return 0;
}
