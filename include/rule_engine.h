#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RULE_ENGINE_MAX_RULES 32u
#define RULE_ENGINE_MAX_SIGNALS 64u
#define RULE_SIGNAL_KEY_MAX 48u
#define RULE_RELAY_COUNT 2u

typedef enum {
  RULE_OP_GT = 0,
  RULE_OP_GE,
  RULE_OP_LT,
  RULE_OP_LE,
  RULE_OP_EQ,
  RULE_OP_NE,
  RULE_OP_HYSTERESIS_HIGH,
  RULE_OP_HYSTERESIS_LOW,
} RuleOp;

typedef enum {
  RELAY_STATE_OFF = 0,
  RELAY_STATE_ON = 1,
} RelayState;

typedef struct {
  char key[RULE_SIGNAL_KEY_MAX];
  double value;
  uint32_t updated_ms;
  bool valid;
} SignalSnapshot;

typedef struct {
  char id[16];
  bool enabled;
  char signal_key[RULE_SIGNAL_KEY_MAX];
  RuleOp op;
  double threshold;
  double on_threshold;
  double off_threshold;
  uint8_t relay;
  RelayState action_state;
  uint32_t delay_ms;
  uint32_t timeout_ms;
  RelayState safe_state;
  RelayState default_state;
  uint8_t priority;
  bool latched_state;
  uint32_t condition_since_ms;
} Rule;

typedef struct {
  bool enabled;
  RelayState relay[RULE_RELAY_COUNT];
} RelayManualOverride;

typedef struct {
  Rule rules[RULE_ENGINE_MAX_RULES];
  size_t rule_count;
  RelayState relay_defaults[RULE_RELAY_COUNT];
  RelayManualOverride manual;
  uint8_t winner_rule[RULE_RELAY_COUNT];
} RuleEngine;

void rule_engine_init(RuleEngine *engine);
bool rule_engine_add_rule(RuleEngine *engine, const Rule *rule);
void rule_engine_set_manual(RuleEngine *engine, bool enabled, const RelayState relay[RULE_RELAY_COUNT]);
void rule_engine_evaluate(RuleEngine *engine,
                          const SignalSnapshot *signals,
                          size_t signal_count,
                          uint32_t now_ms,
                          RelayState out_relays[RULE_RELAY_COUNT]);

#endif
