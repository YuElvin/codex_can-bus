#include "dbc_parser.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(expr)                                                                    \
  do {                                                                                       \
    if (!(expr)) {                                                                           \
      printf("ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_EQ_SIZE(expected, actual)                                                     \
  do {                                                                                       \
    if ((expected) != (actual)) {                                                            \
      printf("ASSERT_EQ_SIZE failed at %s:%d: expected %zu got %zu\n",                       \
             __FILE__,                                                                       \
             __LINE__,                                                                       \
             (size_t)(expected),                                                             \
             (size_t)(actual));                                                              \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

int main(void) {
  DbcDatabase db;
  dbc_init(&db);

  ASSERT_TRUE(dbc_parse_line(&db, "VERSION \"sample\""));
  ASSERT_TRUE(dbc_parse_line(&db, "BO_ 256 EngineData: 8 Vector__XXX"));
  ASSERT_TRUE(dbc_parse_line(&db, " SG_ rpm : 0|16@1+ (0.125,0) [0|8000] \"rpm\" Vector__XXX"));
  ASSERT_TRUE(dbc_parse_line(&db, " SG_ temp : 16|8@1- (1,-40) [-40|215] \"degC\" Vector__XXX"));

  ASSERT_EQ_SIZE(1, db.message_count);
  ASSERT_EQ_SIZE(2, db.signal_count);
  ASSERT_EQ_SIZE(1, db.skipped_lines);
  ASSERT_EQ_SIZE(0, db.error_lines);

  const DbcMessage *message = dbc_find_message(&db, 256);
  ASSERT_TRUE(message != NULL);
  ASSERT_TRUE(strcmp(message->name, "EngineData") == 0);

  const DbcSignal *rpm = dbc_find_signal(&db, message, "rpm");
  ASSERT_TRUE(rpm != NULL);

  CanFrame frame = {
    .id = 256,
    .ide = CAN_ID_STANDARD,
    .fd = false,
    .brs = false,
    .dlc = 8,
    .data = {0x80, 0x3e},
  };

  double value = 0.0;
  ASSERT_TRUE(dbc_decode_signal_value(rpm, &frame, &value));
  ASSERT_TRUE(value == 2000.0);

  memset(frame.data, 0, sizeof(frame.data));
  ASSERT_TRUE(dbc_encode_signal_value(rpm, &frame, 2000.0));
  ASSERT_TRUE(frame.data[0] == 0x80);
  ASSERT_TRUE(frame.data[1] == 0x3e);
  return 0;
}
