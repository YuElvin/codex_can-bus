#ifndef CAN_TYPES_H
#define CAN_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_FRAME_MAX_DATA_LEN 64u

typedef enum {
  CAN_ID_STANDARD = 0,
  CAN_ID_EXTENDED = 1,
} CanIdType;

typedef struct {
  uint32_t id;
  CanIdType ide;
  bool fd;
  bool brs;
  uint8_t dlc;
  uint8_t data[CAN_FRAME_MAX_DATA_LEN];
  uint64_t timestamp_us;
} CanFrame;

#endif
