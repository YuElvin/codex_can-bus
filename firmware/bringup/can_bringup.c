#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include "platform/stm32h750_bringup.h"
#include "dbc_decoder.h"

#include "FreeRTOS.h"
#include "task.h"

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

volatile uint32_t g_can_tx_count;
volatile uint32_t g_can_rx_count;
volatile uint32_t g_can_error_count;
volatile uint32_t g_can_bus_off;
volatile uint32_t g_can_tec;
volatile uint32_t g_can_rec;
volatile uint32_t g_can_rx_id;
volatile uint32_t g_can_rx_dlc;
volatile uint32_t g_can_rx_first_byte;
volatile uint32_t g_can_external_tx_count;
volatile uint32_t g_can_external_rx_count;
volatile uint32_t g_can_external_error_count;
volatile uint32_t g_can_external_bus_off;
volatile uint32_t g_can_external_tec;
volatile uint32_t g_can_external_rec;
volatile uint32_t g_can_external_rx_id;
volatile uint32_t g_can_external_rx_dlc;
volatile uint32_t g_can_external_rx_first_byte;
volatile uint32_t g_can2_tx_count;
volatile uint32_t g_can2_rx_count;
volatile uint32_t g_can2_error_count;
volatile uint32_t g_can2_bus_off;
volatile uint32_t g_can2_tec;
volatile uint32_t g_can2_rec;
volatile uint32_t g_can2_rx_id;
volatile uint32_t g_can2_rx_dlc;
volatile uint32_t g_can2_rx_first_byte;
volatile uint32_t g_can2_send_result;
volatile uint32_t g_can2_poll_count;
volatile uint32_t g_can2_tx_sequence;
volatile uint32_t g_can2_dbc_decode_attempt_count;
volatile uint32_t g_can2_dbc_matched_frame_count;
volatile uint32_t g_can2_dbc_signal_update_count;
volatile uint32_t g_can2_dbc_decode_error_count;
volatile uint32_t g_can2_dbc_cache_count;
volatile uint32_t g_can2_dbc_last_message_id;
volatile uint32_t g_can2_dbc_tx_self_test_frame_count;
volatile uint32_t g_can2_dbc_rx_frame_count;

static Stm32FdcanContext g_can2_ctx;
static CanPort g_can2_port;
static SignalCache g_can2_signal_cache;
static SignalCache g_can2_tx_self_test_signal_cache;

static const CanFrame k_fd_probe_frame = {
  .id = 0x18ff50e5u,
  .ide = CAN_ID_EXTENDED,
  .fd = true,
  .brs = true,
  .dlc = 12u,
  .data = {
    0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
    0x99u, 0xaau, 0xbbu, 0xccu,
  },
};

static const CanFrame k_classic_probe_frame = {
  .id = 0x123u,
  .ide = CAN_ID_STANDARD,
  .fd = false,
  .brs = false,
  .dlc = 8u,
  .data = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u},
};

static const CanFrame k_can2_analyzer_frame = {
  .id = 0x321u,
  .ide = CAN_ID_STANDARD,
  .fd = false,
  .brs = false,
  .dlc = 8u,
  .data = {0xc2u, 0xa5u, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u},
};

static void capture_status(CanPort *can) {
  CanPortStatus status;
  if (can_port_get_status(can, &status) == CAN_PORT_OK) {
    g_can_tx_count = status.tx_count;
    g_can_rx_count = status.rx_count;
    g_can_error_count = status.error_count;
    g_can_bus_off = status.bus_off ? 1u : 0u;
    g_can_tec = status.tec;
    g_can_rec = status.rec;
  }
}

static void capture_external_status(CanPort *can) {
  CanPortStatus status;
  if (can_port_get_status(can, &status) == CAN_PORT_OK) {
    g_can_external_tx_count = status.tx_count;
    g_can_external_rx_count = status.rx_count;
    g_can_external_error_count = status.error_count;
    g_can_external_bus_off = status.bus_off ? 1u : 0u;
    g_can_external_tec = status.tec;
    g_can_external_rec = status.rec;
  }
}

static void capture_can2_status(void) {
  CanPortStatus status;
  if (can_port_get_status(&g_can2_port, &status) == CAN_PORT_OK) {
    g_can2_tx_count = status.tx_count;
    g_can2_rx_count = status.rx_count;
    g_can2_error_count = status.error_count;
    g_can2_bus_off = status.bus_off ? 1u : 0u;
    g_can2_tec = status.tec;
    g_can2_rec = status.rec;
  }
}

static void decode_can2_frame(const CanFrame *rx, bool tx_self_test) {
  const DbcDatabase *db = w5500_http_active_dbc_snapshot();
  SignalCache *signal_cache = tx_self_test ? &g_can2_tx_self_test_signal_cache : &g_can2_signal_cache;
  if (db == NULL) {
    return;
  }

  ++g_can2_dbc_decode_attempt_count;
  if (tx_self_test) {
    ++g_can2_dbc_tx_self_test_frame_count;
  } else {
    ++g_can2_dbc_rx_frame_count;
  }
  const DbcMessage *message = dbc_find_message(db, rx->id);
  if (message == NULL) {
    return;
  }

  ++g_can2_dbc_matched_frame_count;
  const size_t updated = dbc_decode_frame_to_signal_cache(db, rx, signal_cache, HAL_GetTick());
  g_can2_dbc_signal_update_count += (uint32_t)updated;
  if (!tx_self_test) {
    g_can2_dbc_cache_count = (uint32_t)g_can2_signal_cache.count;
  }
  g_can2_dbc_last_message_id = rx->id;
  if (updated != message->signal_count) {
    ++g_can2_dbc_decode_error_count;
  }
}

static void record_rx(const CanFrame *rx, bool external) {
  if (external) {
    g_can_external_rx_id = rx->id;
    g_can_external_rx_dlc = rx->dlc;
    g_can_external_rx_first_byte = rx->data[0];
  } else {
    g_can_rx_id = rx->id;
    g_can_rx_dlc = rx->dlc;
    g_can_rx_first_byte = rx->data[0];
  }
}

static bool classic_frame_matches(const CanFrame *rx) {
  return rx->id == k_classic_probe_frame.id && rx->dlc == k_classic_probe_frame.dlc &&
         rx->data[0] == k_classic_probe_frame.data[0];
}

static bool fd_frame_matches(const CanFrame *rx) {
  return rx->id == k_fd_probe_frame.id && rx->ide == k_fd_probe_frame.ide &&
         rx->fd == k_fd_probe_frame.fd && rx->brs == k_fd_probe_frame.brs &&
         rx->dlc == k_fd_probe_frame.dlc && rx->data[0] == k_fd_probe_frame.data[0] &&
         rx->data[11] == k_fd_probe_frame.data[11];
}

static int run_loopback_probe(CanPort *can, bool external) {
  if (can_port_send(can, &k_classic_probe_frame) != CAN_PORT_OK) {
    external ? capture_external_status(can) : capture_status(can);
    return 3;
  }
  if (can_port_send(can, &k_fd_probe_frame) != CAN_PORT_OK) {
    external ? capture_external_status(can) : capture_status(can);
    return 4;
  }

  bool classic_seen = false;
  bool fd_seen = false;
  for (uint32_t i = 0u; i < 1000000u; ++i) {
    CanFrame rx;
    if (can_port_receive(can, &rx) == CAN_PORT_OK) {
      record_rx(&rx, external);
      if (classic_frame_matches(&rx)) {
        classic_seen = true;
      }
      if (fd_frame_matches(&rx)) {
        fd_seen = true;
      }
      if (classic_seen && fd_seen) {
        external ? capture_external_status(can) : capture_status(can);
        return 0;
      }
    }
  }
  external ? capture_external_status(can) : capture_status(can);
  return 5;
}

int can_bringup_run(void) {
  Stm32FdcanContext ctx;
  CanPort can;
  stm32h750_fdcan_bind(&can, &ctx, &hfdcan1);

  const CanPortConfig config = {
    .nominal_bitrate = 500000u,
    .data_bitrate = 2000000u,
    .fd_enabled = true,
    .brs_enabled = true,
    .internal_loopback = true,
    .auto_retransmission = true,
  };
  if (can_port_configure(&can, &config) != CAN_PORT_OK) {
    return 1;
  }
  if (can_port_start(&can) != CAN_PORT_OK) {
    return 2;
  }

  return run_loopback_probe(&can, false);
}

int can_external_bringup_run(void) {
  Stm32FdcanContext ctx;
  CanPort can;
  stm32h750_fdcan_bind(&can, &ctx, &hfdcan1);

  const CanPortConfig config = {
    .nominal_bitrate = 500000u,
    .data_bitrate = 2000000u,
    .fd_enabled = true,
    .brs_enabled = true,
    .external_loopback = true,
    .auto_retransmission = false,
  };
  if (can_port_configure(&can, &config) != CAN_PORT_OK) {
    return 1;
  }
  if (can_port_start(&can) != CAN_PORT_OK) {
    return 2;
  }

  return run_loopback_probe(&can, true);
}

int can2_analyzer_bringup_run(void) {
  stm32h750_fdcan_bind(&g_can2_port, &g_can2_ctx, &hfdcan2);
  signal_cache_init(&g_can2_signal_cache);
  signal_cache_init(&g_can2_tx_self_test_signal_cache);

  const CanPortConfig config = {
    .nominal_bitrate = 500000u,
    .data_bitrate = 2000000u,
    .fd_enabled = false,
    .brs_enabled = false,
    .auto_retransmission = true,
  };
  if (can_port_configure(&g_can2_port, &config) != CAN_PORT_OK) {
    return 1;
  }
  if (can_port_start(&g_can2_port) != CAN_PORT_OK) {
    return 2;
  }

  g_can2_send_result = can_port_send(&g_can2_port, &k_can2_analyzer_frame);
  capture_can2_status();
  return g_can2_send_result == CAN_PORT_OK ? 0 : 3;
}

int can2_analyzer_receive(void) {
  CanFrame rx;

  while (can_port_receive(&g_can2_port, &rx) == CAN_PORT_OK) {
    g_can2_rx_id = rx.id;
    g_can2_rx_dlc = rx.dlc;
    g_can2_rx_first_byte = rx.data[0];
    decode_can2_frame(&rx, false);
  }
  capture_can2_status();
  return 0;
}

int can2_analyzer_poll(void) {
  CanFrame tx = k_can2_analyzer_frame;

  ++g_can2_poll_count;
  tx.data[2] = (uint8_t)g_can2_tx_sequence;
  tx.data[3] = (uint8_t)(g_can2_tx_sequence >> 8);
  ++g_can2_tx_sequence;

  g_can2_send_result = can_port_send(&g_can2_port, &tx);
  if (g_can2_send_result == CAN_PORT_OK) {
    decode_can2_frame(&tx, true);
  }
  (void)can2_analyzer_receive();
  return g_can2_send_result == CAN_PORT_OK ? 0 : 1;
}

size_t can2_signal_cache_copy(SignalCacheEntry *out_entries, size_t out_capacity) {
  size_t count;
  taskENTER_CRITICAL();
  count = signal_cache_copy(&g_can2_signal_cache, out_entries, out_capacity);
  taskEXIT_CRITICAL();
  return count;
}

size_t can2_signal_cache_export_rule_snapshots(SignalSnapshot *out_signals, size_t out_capacity) {
  size_t count;
  taskENTER_CRITICAL();
  count = signal_cache_export_rule_snapshots(&g_can2_signal_cache, out_signals, out_capacity);
  taskEXIT_CRITICAL();
  return count;
}

#endif
