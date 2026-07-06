#if defined(CAN_BUS_USE_STM32_HAL) || defined(STM32H750xx)

#include <stdint.h>

#include "platform/stm32h750_bringup.h"

extern QSPI_HandleTypeDef hqspi;

volatile uint32_t g_w25q128_jedec_id = 0xffffffffu;
volatile uint32_t g_w25q128_status_reg1 = 0xffffffffu;
volatile uint32_t g_w25q128_test_addr = 0x00fff000u;
volatile uint32_t g_w25q128_mismatch_index = 0xffffffffu;
volatile uint32_t g_w25q128_expected = 0xffffffffu;
volatile uint32_t g_w25q128_actual = 0xffffffffu;
volatile uint32_t g_w25q128_last_hal_status = 0xffffffffu;

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
#define W25Q128_QSPI_TIMEOUT_MS        1000u
#define W25Q128_ERASE_TIMEOUT_MS       5000u

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
  static const uint8_t pattern[W25Q128_TEST_LEN] = {
    0x57u, 0x32u, 0x35u, 0x51u, 0x31u, 0x32u, 0x38u, 0x00u,
    0xa5u, 0x5au, 0x00u, 0xffu, 0x13u, 0x57u, 0x9bu, 0xdfu,
    0x10u, 0x32u, 0x54u, 0x76u, 0x98u, 0xbau, 0xdcu, 0xfeu,
    0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xabu, 0xcdu, 0xefu,
  };
  uint8_t id[3] = {0};
  uint8_t readback[W25Q128_TEST_LEN] = {0};

  g_w25q128_test_addr = W25Q128_TEST_ADDR;
  g_w25q128_mismatch_index = 0xffffffffu;
  g_w25q128_expected = 0xffffffffu;
  g_w25q128_actual = 0xffffffffu;

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

#endif
