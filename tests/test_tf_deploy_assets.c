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

int main(void) {
  FILE *file = fopen("deploy/tf/dbc/active.dbc", "rb");
  char text[1025];
  DbcDatabase db;
  size_t line_count = 0u;

  ASSERT_TRUE(file != NULL);
  const size_t len = fread(text, 1u, sizeof(text), file);
  ASSERT_TRUE(ferror(file) == 0);
  ASSERT_TRUE(fclose(file) == 0);
  ASSERT_TRUE(len == 151u);

  dbc_init(&db);
  ASSERT_TRUE(dbc_parse_text(&db, text, len, &line_count));
  ASSERT_TRUE(line_count == 3u);
  ASSERT_TRUE(db.message_count == 1u);
  ASSERT_TRUE(db.signal_count == 2u);
  ASSERT_TRUE(db.skipped_lines == 0u);
  ASSERT_TRUE(db.error_lines == 0u);

  const DbcMessage *message = dbc_find_message(&db, 0x321u);
  ASSERT_TRUE(message != NULL);
  ASSERT_TRUE(message->dlc == 8u);
  ASSERT_TRUE(strcmp(message->name, "Can2Data") == 0);
  ASSERT_TRUE(dbc_find_signal(&db, message, "marker") != NULL);
  ASSERT_TRUE(dbc_find_signal(&db, message, "sequence") != NULL);
  return 0;
}
