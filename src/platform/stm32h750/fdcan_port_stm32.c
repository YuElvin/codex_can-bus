#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"

static uint32_t bytes_to_dlc(uint8_t bytes) {
  static const uint32_t dlc_map[] = {
    FDCAN_DLC_BYTES_0,
    FDCAN_DLC_BYTES_1,
    FDCAN_DLC_BYTES_2,
    FDCAN_DLC_BYTES_3,
    FDCAN_DLC_BYTES_4,
    FDCAN_DLC_BYTES_5,
    FDCAN_DLC_BYTES_6,
    FDCAN_DLC_BYTES_7,
    FDCAN_DLC_BYTES_8,
  };
  if (bytes <= 8u) {
    return dlc_map[bytes];
  }
  if (bytes <= 12u) {
    return FDCAN_DLC_BYTES_12;
  }
  if (bytes <= 16u) {
    return FDCAN_DLC_BYTES_16;
  }
  if (bytes <= 20u) {
    return FDCAN_DLC_BYTES_20;
  }
  if (bytes <= 24u) {
    return FDCAN_DLC_BYTES_24;
  }
  if (bytes <= 32u) {
    return FDCAN_DLC_BYTES_32;
  }
  if (bytes <= 48u) {
    return FDCAN_DLC_BYTES_48;
  }
  return FDCAN_DLC_BYTES_64;
}

static uint8_t dlc_to_bytes(uint32_t dlc) {
  switch (dlc) {
    case FDCAN_DLC_BYTES_0:
      return 0u;
    case FDCAN_DLC_BYTES_1:
      return 1u;
    case FDCAN_DLC_BYTES_2:
      return 2u;
    case FDCAN_DLC_BYTES_3:
      return 3u;
    case FDCAN_DLC_BYTES_4:
      return 4u;
    case FDCAN_DLC_BYTES_5:
      return 5u;
    case FDCAN_DLC_BYTES_6:
      return 6u;
    case FDCAN_DLC_BYTES_7:
      return 7u;
    case FDCAN_DLC_BYTES_8:
      return 8u;
    case FDCAN_DLC_BYTES_12:
      return 12u;
    case FDCAN_DLC_BYTES_16:
      return 16u;
    case FDCAN_DLC_BYTES_20:
      return 20u;
    case FDCAN_DLC_BYTES_24:
      return 24u;
    case FDCAN_DLC_BYTES_32:
      return 32u;
    case FDCAN_DLC_BYTES_48:
      return 48u;
    default:
      return 64u;
  }
}

static CanPortResult fdcan_configure(void *ctx, const CanPortConfig *config) {
  Stm32FdcanContext *fdcan = (Stm32FdcanContext *)ctx;
  if (fdcan == NULL || fdcan->hfdcan == NULL || config == NULL) {
    return CAN_PORT_ERROR;
  }

  if (config->internal_loopback) {
    if (HAL_FDCAN_ConfigGlobalFilter(fdcan->hfdcan,
                                     FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_REJECT_REMOTE,
                                     FDCAN_REJECT_REMOTE) != HAL_OK) {
      return CAN_PORT_ERROR;
    }
  }
  return CAN_PORT_OK;
}

static CanPortResult fdcan_start(void *ctx) {
  Stm32FdcanContext *fdcan = (Stm32FdcanContext *)ctx;
  if (fdcan == NULL || fdcan->hfdcan == NULL) {
    return CAN_PORT_ERROR;
  }
  return HAL_FDCAN_Start(fdcan->hfdcan) == HAL_OK ? CAN_PORT_OK : CAN_PORT_ERROR;
}

static CanPortResult fdcan_send(void *ctx, const CanFrame *frame) {
  Stm32FdcanContext *fdcan = (Stm32FdcanContext *)ctx;
  FDCAN_TxHeaderTypeDef header = {0};
  if (fdcan == NULL || fdcan->hfdcan == NULL || frame == NULL) {
    return CAN_PORT_ERROR;
  }

  header.Identifier = frame->id;
  header.IdType = frame->ide == CAN_ID_EXTENDED ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  header.TxFrameType = FDCAN_DATA_FRAME;
  header.DataLength = bytes_to_dlc(frame->dlc);
  header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  header.BitRateSwitch = frame->brs ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
  header.FDFormat = frame->fd ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
  header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  header.MessageMarker = 0u;

  return HAL_FDCAN_AddMessageToTxFifoQ(fdcan->hfdcan, &header, (uint8_t *)frame->data) == HAL_OK
           ? CAN_PORT_OK
           : CAN_PORT_ERROR;
}

static CanPortResult fdcan_receive(void *ctx, CanFrame *frame) {
  Stm32FdcanContext *fdcan = (Stm32FdcanContext *)ctx;
  FDCAN_RxHeaderTypeDef header = {0};
  if (fdcan == NULL || fdcan->hfdcan == NULL || frame == NULL) {
    return CAN_PORT_ERROR;
  }
  if (HAL_FDCAN_GetRxFifoFillLevel(fdcan->hfdcan, FDCAN_RX_FIFO0) == 0u) {
    return CAN_PORT_RX_EMPTY;
  }
  if (HAL_FDCAN_GetRxMessage(fdcan->hfdcan, FDCAN_RX_FIFO0, &header, frame->data) != HAL_OK) {
    return CAN_PORT_ERROR;
  }

  frame->id = header.Identifier;
  frame->ide = header.IdType == FDCAN_EXTENDED_ID ? CAN_ID_EXTENDED : CAN_ID_STANDARD;
  frame->fd = header.FDFormat == FDCAN_FD_CAN;
  frame->brs = header.BitRateSwitch == FDCAN_BRS_ON;
  frame->dlc = dlc_to_bytes(header.DataLength);
  return CAN_PORT_OK;
}

static CanPortResult fdcan_status(void *ctx, CanPortStatus *status) {
  Stm32FdcanContext *fdcan = (Stm32FdcanContext *)ctx;
  FDCAN_ProtocolStatusTypeDef protocol = {0};
  if (fdcan == NULL || fdcan->hfdcan == NULL || status == NULL) {
    return CAN_PORT_ERROR;
  }
  if (HAL_FDCAN_GetProtocolStatus(fdcan->hfdcan, &protocol) != HAL_OK) {
    return CAN_PORT_ERROR;
  }
  status->bus_off = protocol.BusOff;
  status->tec = (uint8_t)protocol.LastErrorCode;
  status->rec = (uint8_t)protocol.DataLastErrorCode;
  return CAN_PORT_OK;
}

void stm32h750_fdcan_bind(CanPort *port, Stm32FdcanContext *ctx, FDCAN_HandleTypeDef *hfdcan) {
  static const CanPortOps ops = {
    .configure = fdcan_configure,
    .start = fdcan_start,
    .send = fdcan_send,
    .receive = fdcan_receive,
    .get_status = fdcan_status,
  };
  ctx->hfdcan = hfdcan;
  can_port_bind(port, ctx, &ops);
}

#endif
