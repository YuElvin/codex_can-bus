#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include <stdint.h>
#include <string.h>

#include "platform/stm32h750_bringup.h"

extern QSPI_HandleTypeDef hqspi;

volatile uint32_t g_w25q128_jedec_id = 0xffffffffu;
volatile uint32_t g_w25q128_status_reg1 = 0xffffffffu;
volatile uint32_t g_w25q128_test_addr = 0xffffffffu;
volatile uint32_t g_w25q128_mismatch_index = 0xffffffffu;
volatile uint32_t g_w25q128_expected = 0xffffffffu;
volatile uint32_t g_w25q128_actual = 0xffffffffu;
volatile uint32_t g_w25q128_last_hal_status = 0xffffffffu;
volatile uint32_t g_w25q128_diagnostic_request;
volatile uint32_t g_w25q128_diagnostic_result = 0xffffffffu;
volatile uint32_t g_w25q128_diagnostic_count;
volatile uint32_t g_w25q128_erase_count;
volatile uint32_t g_w25q128_config_addr = 0x00ffe000u;
volatile uint32_t g_w25q128_config_load_result = 0xffffffffu;
volatile uint32_t g_w25q128_config_save_result = 0xffffffffu;
volatile uint32_t g_w25q128_config_load_count;
volatile uint32_t g_w25q128_config_save_count;
volatile uint32_t g_w25q128_config_sequence;

#define W25Q128_CMD_WRITE_ENABLE       0x06u
#define W25Q128_CMD_READ_STATUS_REG1   0x05u
#define W25Q128_CMD_READ_DATA          0x03u
#define W25Q128_CMD_PAGE_PROGRAM       0x02u
#define W25Q128_CMD_SECTOR_ERASE       0x20u
#define W25Q128_CMD_READ_JEDEC_ID      0x9fu
#define W25Q128_CMD_RELEASE_POWER_DOWN 0xabu

#define W25Q128_STATUS_BUSY            0x01u
#define W25Q128_STATUS_WEL             0x02u
#define W25Q128_TEST_ADDR              0x00fff000u
#define W25Q128_TEST_LEN               32u
#define W25Q128_CONFIG_PRIMARY_ADDR    0x00ffe000u
#define W25Q128_CONFIG_BACKUP_ADDR     0x00ffd000u
#define W25Q128_CONFIG_MAGIC           0x52434647u
#define W25Q128_CONFIG_VERSION_V1      1u
#define W25Q128_CONFIG_VERSION         2u
#define W25Q128_QSPI_TIMEOUT_MS        1000u
#define W25Q128_ERASE_TIMEOUT_MS       5000u

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t on_threshold;
  uint32_t off_threshold;
  uint32_t delay_ms;
  uint32_t timeout_ms;
  uint32_t sequence;
  uint32_t checksum;
} W25Q128RuleConfigRecord;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t on_threshold;
  uint32_t off_threshold;
  uint32_t delay_ms;
  uint32_t timeout_ms;
  uint32_t checksum;
} W25Q128RuleConfigRecordV1;

static uint32_t w25q128_rule_config_checksum(const W25Q128RuleConfigRecord *record)
{
  return record->magic ^ record->version ^ record->on_threshold ^ record->off_threshold ^
         record->delay_ms ^ record->timeout_ms ^ record->sequence ^ 0xa5c35a3cu;
}

static uint32_t w25q128_rule_config_v1_checksum(const W25Q128RuleConfigRecordV1 *record)
{
  return record->magic ^ record->version ^ record->on_threshold ^ record->off_threshold ^
         record->delay_ms ^ record->timeout_ms ^ 0xa5c35a3cu;
}

static HAL_StatusTypeDef w25q128_command(uint32_t instruction,
                                         uint32_t address_mode,
                                         uint32_t address,
                                         uint32_t data_mode,
                                         uint32_t data_len)
{
  QSPI_CommandTypeDef command = {0};
  command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
  command.Instruction = instruction;
  command.AddressMode = address_mode;
  command.AddressSize = QSPI_ADDRESS_24_BITS;
  command.Address = address;
  command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = data_mode;
  command.NbData = data_len;
  command.DummyCycles = 0;
  command.DdrMode = QSPI_DDR_MODE_DISABLE;
  command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
  command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

  const HAL_StatusTypeDef status = HAL_QSPI_Command(&hqspi, &command, W25Q128_QSPI_TIMEOUT_MS);
  g_w25q128_last_hal_status = (uint32_t)status;
  return status;
}

static int w25q128_read_status(uint8_t *status_reg)
{
  if (status_reg == NULL) {
    return 1;
  }

  if (w25q128_command(W25Q128_CMD_READ_STATUS_REG1,
                      QSPI_ADDRESS_NONE,
                      0u,
                      QSPI_DATA_1_LINE,
                      1u) != HAL_OK) {
    return 1;
  }

  const HAL_StatusTypeDef status = HAL_QSPI_Receive(&hqspi, status_reg, W25Q128_QSPI_TIMEOUT_MS);
  g_w25q128_last_hal_status = (uint32_t)status;
  if (status != HAL_OK) {
    return 1;
  }

  g_w25q128_status_reg1 = *status_reg;
  return 0;
}

static int w25q128_wait_ready(uint32_t timeout_ms)
{
  const uint32_t start = HAL_GetTick();
  uint8_t status_reg = 0xffu;

  do {
    if (w25q128_read_status(&status_reg) != 0) {
      return 1;
    }
    if ((status_reg & W25Q128_STATUS_BUSY) == 0u) {
      return 0;
    }
  } while ((HAL_GetTick() - start) < timeout_ms);

  return 1;
}

static int w25q128_write_enable(void)
{
  uint8_t status_reg = 0u;

  if (w25q128_command(W25Q128_CMD_WRITE_ENABLE,
                      QSPI_ADDRESS_NONE,
                      0u,
                      QSPI_DATA_NONE,
                      0u) != HAL_OK) {
    return 1;
  }

  if (w25q128_read_status(&status_reg) != 0) {
    return 1;
  }

  return (status_reg & W25Q128_STATUS_WEL) != 0u ? 0 : 1;
}

static int w25q128_read(uint32_t address, uint8_t *data, uint32_t length)
{
  if (w25q128_command(W25Q128_CMD_READ_DATA,
                      QSPI_ADDRESS_1_LINE,
                      address,
                      QSPI_DATA_1_LINE,
                      length) != HAL_OK) {
    return 1;
  }

  const HAL_StatusTypeDef status = HAL_QSPI_Receive(&hqspi, data, W25Q128_QSPI_TIMEOUT_MS);
  g_w25q128_last_hal_status = (uint32_t)status;
  return status == HAL_OK ? 0 : 1;
}

static int w25q128_sector_erase(uint32_t address)
{
  if (w25q128_write_enable() != 0) {
    return 1;
  }

  if (w25q128_command(W25Q128_CMD_SECTOR_ERASE,
                      QSPI_ADDRESS_1_LINE,
                      address,
                      QSPI_DATA_NONE,
                      0u) != HAL_OK) {
    return 1;
  }

  ++g_w25q128_erase_count;
  return w25q128_wait_ready(W25Q128_ERASE_TIMEOUT_MS);
}

static int w25q128_program(uint32_t address, const uint8_t *data, uint32_t length)
{
  if (w25q128_write_enable() != 0) {
    return 1;
  }

  if (w25q128_command(W25Q128_CMD_PAGE_PROGRAM,
                      QSPI_ADDRESS_1_LINE,
                      address,
                      QSPI_DATA_1_LINE,
                      length) != HAL_OK) {
    return 1;
  }

  const HAL_StatusTypeDef status = HAL_QSPI_Transmit(&hqspi, (uint8_t *)data, W25Q128_QSPI_TIMEOUT_MS);
  g_w25q128_last_hal_status = (uint32_t)status;
  if (status != HAL_OK) {
    return 1;
  }

  return w25q128_wait_ready(W25Q128_QSPI_TIMEOUT_MS);
}

static int w25q128_read_jedec_id(uint8_t id[3])
{
  if (w25q128_command(W25Q128_CMD_READ_JEDEC_ID,
                      QSPI_ADDRESS_NONE,
                      0u,
                      QSPI_DATA_1_LINE,
                      3u) != HAL_OK) {
    return 1;
  }

  const HAL_StatusTypeDef status = HAL_QSPI_Receive(&hqspi, id, W25Q128_QSPI_TIMEOUT_MS);
  g_w25q128_last_hal_status = (uint32_t)status;
  if (status != HAL_OK) {
    return 1;
  }

  g_w25q128_jedec_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
  return 0;
}

int w25q128_bringup_run(void)
{
  uint8_t id[3] = {0};

  (void)w25q128_command(W25Q128_CMD_RELEASE_POWER_DOWN,
                        QSPI_ADDRESS_NONE,
                        0u,
                        QSPI_DATA_NONE,
                        0u);
  HAL_Delay(1u);

  if (w25q128_wait_ready(W25Q128_QSPI_TIMEOUT_MS) != 0) {
    return 1;
  }

  if (w25q128_read_jedec_id(id) != 0) {
    return 1;
  }

  if (id[0] != 0xefu || id[2] != 0x18u) {
    return 2;
  }

  return 0;
}

int w25q128_diagnostic_run(void)
{
  static const uint8_t pattern[W25Q128_TEST_LEN] = {
    0x57u, 0x32u, 0x35u, 0x51u, 0x31u, 0x32u, 0x38u, 0x00u,
    0xa5u, 0x5au, 0x00u, 0xffu, 0x13u, 0x57u, 0x9bu, 0xdfu,
    0x10u, 0x32u, 0x54u, 0x76u, 0x98u, 0xbau, 0xdcu, 0xfeu,
    0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xabu, 0xcdu, 0xefu,
  };
  uint8_t readback[W25Q128_TEST_LEN] = {0};

  g_w25q128_test_addr = W25Q128_TEST_ADDR;
  g_w25q128_mismatch_index = 0xffffffffu;
  g_w25q128_expected = 0xffffffffu;
  g_w25q128_actual = 0xffffffffu;

  if (w25q128_bringup_run() != 0) {
    return 1;
  }

  if (w25q128_sector_erase(W25Q128_TEST_ADDR) != 0) {
    return 3;
  }

  if (w25q128_program(W25Q128_TEST_ADDR, pattern, W25Q128_TEST_LEN) != 0) {
    return 4;
  }

  if (w25q128_read(W25Q128_TEST_ADDR, readback, W25Q128_TEST_LEN) != 0) {
    return 5;
  }

  for (uint32_t index = 0; index < W25Q128_TEST_LEN; ++index) {
    if (readback[index] != pattern[index]) {
      g_w25q128_mismatch_index = index;
      g_w25q128_expected = pattern[index];
      g_w25q128_actual = readback[index];
      return 5;
    }
  }

  return 0;
}

static int w25q128_rule_config_values_valid(uint32_t on_threshold,
                                             uint32_t off_threshold,
                                             uint32_t delay_ms,
                                             uint32_t timeout_ms)
{
  return on_threshold > off_threshold && delay_ms <= timeout_ms ? 0 : 1;
}

static int w25q128_rule_config_read_v2(uint32_t address, W25Q128RuleConfigRecord *record)
{
  if (w25q128_read(address, (uint8_t *)record, sizeof(*record)) != 0 ||
      record->magic != W25Q128_CONFIG_MAGIC || record->version != W25Q128_CONFIG_VERSION ||
      record->checksum != w25q128_rule_config_checksum(record) ||
      w25q128_rule_config_values_valid(record->on_threshold,
                                       record->off_threshold,
                                       record->delay_ms,
                                       record->timeout_ms) != 0) {
    return 1;
  }
  return 0;
}

static int w25q128_rule_config_read_v1(W25Q128RuleConfigRecordV1 *record)
{
  if (w25q128_read(W25Q128_CONFIG_PRIMARY_ADDR, (uint8_t *)record, sizeof(*record)) != 0 ||
      record->magic != W25Q128_CONFIG_MAGIC || record->version != W25Q128_CONFIG_VERSION_V1 ||
      record->checksum != w25q128_rule_config_v1_checksum(record) ||
      w25q128_rule_config_values_valid(record->on_threshold,
                                       record->off_threshold,
                                       record->delay_ms,
                                       record->timeout_ms) != 0) {
    return 1;
  }
  return 0;
}

static int w25q128_rule_config_current(W25Q128RuleConfigRecord *current, uint32_t *address)
{
  W25Q128RuleConfigRecord primary = {0};
  W25Q128RuleConfigRecord backup = {0};
  W25Q128RuleConfigRecordV1 legacy = {0};
  const int primary_valid = w25q128_rule_config_read_v2(W25Q128_CONFIG_PRIMARY_ADDR, &primary) == 0;
  const int backup_valid = w25q128_rule_config_read_v2(W25Q128_CONFIG_BACKUP_ADDR, &backup) == 0;

  if (primary_valid || backup_valid) {
    if (!backup_valid || (primary_valid && primary.sequence >= backup.sequence)) {
      *current = primary;
      *address = W25Q128_CONFIG_PRIMARY_ADDR;
    } else {
      *current = backup;
      *address = W25Q128_CONFIG_BACKUP_ADDR;
    }
    return 0;
  }
  if (w25q128_rule_config_read_v1(&legacy) == 0) {
    current->magic = legacy.magic;
    current->version = legacy.version;
    current->on_threshold = legacy.on_threshold;
    current->off_threshold = legacy.off_threshold;
    current->delay_ms = legacy.delay_ms;
    current->timeout_ms = legacy.timeout_ms;
    current->sequence = 0u;
    current->checksum = legacy.checksum;
    *address = W25Q128_CONFIG_PRIMARY_ADDR;
    return 0;
  }
  return 1;
}

int w25q128_rule_config_load(uint32_t *on_threshold,
                              uint32_t *off_threshold,
                              uint32_t *delay_ms,
                              uint32_t *timeout_ms)
{
  W25Q128RuleConfigRecord current = {0};
  uint32_t address = W25Q128_CONFIG_PRIMARY_ADDR;

  if (on_threshold == NULL || off_threshold == NULL || delay_ms == NULL || timeout_ms == NULL) {
    g_w25q128_config_load_result = 1u;
    return 1;
  }
  if (w25q128_rule_config_current(&current, &address) != 0) {
    g_w25q128_config_load_result = 2u;
    return 2;
  }

  *on_threshold = current.on_threshold;
  *off_threshold = current.off_threshold;
  *delay_ms = current.delay_ms;
  *timeout_ms = current.timeout_ms;
  g_w25q128_config_addr = address;
  g_w25q128_config_sequence = current.sequence;
  g_w25q128_config_load_result = 0u;
  ++g_w25q128_config_load_count;
  return 0;
}

int w25q128_rule_config_save(uint32_t on_threshold,
                              uint32_t off_threshold,
                              uint32_t delay_ms,
                              uint32_t timeout_ms)
{
  W25Q128RuleConfigRecord record = {
    .magic = W25Q128_CONFIG_MAGIC,
    .version = W25Q128_CONFIG_VERSION,
    .on_threshold = on_threshold,
    .off_threshold = off_threshold,
    .delay_ms = delay_ms,
    .timeout_ms = timeout_ms,
  };
  W25Q128RuleConfigRecord readback = {0};
  W25Q128RuleConfigRecord current = {0};
  uint32_t current_addr = W25Q128_CONFIG_PRIMARY_ADDR;
  uint32_t target_addr = W25Q128_CONFIG_PRIMARY_ADDR;

  if (w25q128_rule_config_values_valid(on_threshold, off_threshold, delay_ms, timeout_ms) != 0) {
    g_w25q128_config_save_result = 1u;
    return 1;
  }
  if (w25q128_rule_config_current(&current, &current_addr) == 0) {
    if (current.sequence == 0xffffffffu) {
      g_w25q128_config_save_result = 5u;
      return 5;
    }
    record.sequence = current.sequence + 1u;
    target_addr = current_addr == W25Q128_CONFIG_PRIMARY_ADDR ?
                      W25Q128_CONFIG_BACKUP_ADDR : W25Q128_CONFIG_PRIMARY_ADDR;
  } else {
    record.sequence = 1u;
  }
  record.checksum = w25q128_rule_config_checksum(&record);
  if (w25q128_sector_erase(target_addr) != 0) {
    g_w25q128_config_save_result = 2u;
    return 2;
  }
  if (w25q128_program(target_addr, (const uint8_t *)&record, sizeof(record)) != 0) {
    g_w25q128_config_save_result = 3u;
    return 3;
  }
  if (w25q128_read(target_addr, (uint8_t *)&readback, sizeof(readback)) != 0 ||
      memcmp(&record, &readback, sizeof(record)) != 0) {
    g_w25q128_config_save_result = 4u;
    return 4;
  }

  g_w25q128_config_addr = target_addr;
  g_w25q128_config_sequence = record.sequence;
  g_w25q128_config_save_result = 0u;
  ++g_w25q128_config_save_count;
  return 0;
}

#endif
