/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "modules.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* ========== UTILITIES ========== */

static void DBG(const char *s) {
    HAL_UART_Transmit(&huart2, (uint8_t*)s, strlen(s), 100);
}

static void DBG_Hex(uint32_t val, uint8_t digits) {
    char buf[12];
    sprintf(buf, "%0*lX", digits, (unsigned long)val);
    DBG(buf);
}

// Read line with echo and backspace support
static uint16_t ReadLine(char *buf, uint16_t max_len) {
    uint16_t len = 0;
    buf[0] = '\0';

    while (1) {
        uint8_t c;
        if (HAL_UART_Receive(&huart2, &c, 1, 100) != HAL_OK) continue;

        if (c == '\r' || c == '\n') {
            buf[len] = '\0';
            DBG("\r\n");
            return len;
        } else if (c == '\b' || c == 0x7F) {
            if (len > 0) {
                len--;
                buf[len] = '\0';
                DBG("\b \b");
            }
        } else if (c >= 0x20 && c < 0x7F && len < max_len - 1) {
            buf[len++] = c;
            buf[len] = '\0';
            char echo[2] = {c, 0};
            DBG(echo);
        }
    }
}

static uint32_t ReadHex32(void) {
    char buf[16] = {0};
    ReadLine(buf, sizeof(buf));
    return (uint32_t)strtoul(buf, NULL, 16);
}

static uint16_t ReadHex16(void) {
    char buf[8] = {0};
    ReadLine(buf, sizeof(buf));
    return (uint16_t)strtoul(buf, NULL, 16);
}

/* ========== COMMANDS ========== */

static void Cmd_Info(void) {
    DBG("\r\n[INFO] Getting chip information...\r\n");

    uint8_t ver = 0, pid = 0;
    if (!BL_GetVersion(&ver, &pid)) {
        DBG("[INFO] ERROR! No response\r\n");
        return;
    }

    DBG("[INFO] Bootloader version: 0x");
    DBG_Hex(ver, 2);
    DBG("\r\n[INFO] Product ID: 0x");
    DBG_Hex(pid, 2);
    DBG("\r\n[INFO] Chip: ");

    switch (pid) {
        case 0x04: DBG("STM32F103 Medium-density (64/128 KB)\r\n"); break;
        case 0x0A: DBG("STM32F103 High-density\r\n"); break;
        case 0x18: DBG("STM32F103 Connectivity line\r\n"); break;
        case 0x20: DBG("STM32F103 Value line medium\r\n"); break;
        case 0x22: DBG("STM32F103 Value line high\r\n"); break;
        case 0x30: DBG("STM32F103 XL-density\r\n"); break;
        case 0x41: DBG("STM32F405/407/415/417\r\n"); break;
        case 0x42: DBG("STM32F401xB/C\r\n"); break;
        case 0x43: DBG("STM32F401xD/E\r\n"); break;
        case 0x44: DBG("STM32F405/407 rev.3\r\n"); break;
        case 0x46: DBG("STM32F401xE\r\n"); break;
        default:   DBG("Unknown (check AN2606)\r\n"); break;
    }
}

static void Cmd_Read(void) {
    DBG("\r\n[READ] Enter address (hex, e.g. 08000000): ");
    uint32_t addr = ReadHex32();

    if (addr == 0) {
        DBG("[READ] ERROR: address cannot be 0! Use 08000000 for Flash.\r\n");
        return;
    }

    DBG("[READ] Reading 256 bytes from 0x");
    DBG_Hex(addr, 8);
    DBG("...\r\n");

    uint8_t buffer[256];
    if (!BL_ReadMemory(addr, buffer, 256)) {
        DBG("[READ] READ ERROR! Check address range.\r\n");
        return;
    }

    for (int i = 0; i < 256; i++) {
        char b[4];
        sprintf(b, "%02X ", buffer[i]);
        DBG(b);
        if ((i + 1) % 16 == 0) DBG("\r\n");
    }
    DBG("[READ] OK\r\n");
}

static void Cmd_ReadAll(void) {
    DBG("\r\n[READALL] Reading entire 128 KB Flash...\r\n");

    uint8_t buffer[256];
    uint32_t address = 0x08000000;
    uint32_t end_address = 0x08020000;
    uint32_t block_count = 0;
    char msg[80];

    while (address < end_address) {
        if (BL_ReadMemory(address, buffer, 256)) {
            if (block_count % 32 == 0) {
                sprintf(msg, "[READALL] 0x%08lX (%lu KB)\r\n",
                        (unsigned long)address,
                        (unsigned long)(block_count * 256 / 1024));
                DBG(msg);
            }
            block_count++;
            address += 256;
        } else {
            sprintf(msg, "[READALL] ERROR at 0x%08lX\r\n", (unsigned long)address);
            DBG(msg);
            break;
        }
    }

    sprintf(msg, "[READALL] Done! %lu blocks (%lu KB)\r\n",
            block_count, block_count * 256 / 1024);
    DBG(msg);
}

static void Cmd_Erase(void) {
    DBG("\r\n[ERASE] WARNING! Erasing ENTIRE Flash!\r\n");
    DBG("[ERASE] Type 'YES' to confirm: ");

    char confirm[8] = {0};
    ReadLine(confirm, sizeof(confirm));

    if (strcmp(confirm, "YES") != 0) {
        DBG("[ERASE] Cancelled.\r\n");
        return;
    }

    DBG("[ERASE] Erasing... (1-3 seconds)\r\n");
    if (!BL_EraseMemory()) {
        DBG("[ERASE] ERROR!\r\n");
        return;
    }
    DBG("[ERASE] SUCCESS! Flash erased.\r\n");
}

static void Cmd_Write(void) {
    DBG("\r\n[WRITE] Enter address (hex, e.g. 08000000): ");
    uint32_t addr = ReadHex32();

    DBG("[WRITE] Enter length in hex (e.g. 10=16 bytes, 100=256 bytes): ");
    uint16_t len = ReadHex16();

    if (len == 0 || len > 256) {
        DBG("[WRITE] ERROR: length must be 1-256 (hex)!\r\n");
        return;
    }

    DBG("[WRITE] Enter ");
    DBG_Hex(len, 2);
    DBG(" bytes in HEX (no spaces, e.g. BE000020...): ");

    uint8_t data[256];
    char hex[3] = {0};
    for (uint16_t i = 0; i < len; i++) {
        hex[0] = 0; hex[1] = 0;
        while (hex[0] == 0) {
            uint8_t c;
            if (HAL_UART_Receive(&huart2, &c, 1, 100) != HAL_OK) continue;
            if (c >= '0' && c <= '9') hex[0] = c;
            else if (c >= 'A' && c <= 'F') hex[0] = c;
            else if (c >= 'a' && c <= 'f') hex[0] = c - 32;
        }
        while (hex[1] == 0) {
            uint8_t c;
            if (HAL_UART_Receive(&huart2, &c, 1, 100) != HAL_OK) continue;
            if (c >= '0' && c <= '9') hex[1] = c;
            else if (c >= 'A' && c <= 'F') hex[1] = c;
            else if (c >= 'a' && c <= 'f') hex[1] = c - 32;
        }
        data[i] = (uint8_t)strtoul(hex, NULL, 16);
    }
    DBG("\r\n");

    DBG("[WRITE] Writing 0x");
    DBG_Hex(len, 2);
    DBG(" bytes to 0x");
    DBG_Hex(addr, 8);
    DBG("...\r\n");

    if (!BL_WriteMemory(addr, data, len)) {
        DBG("[WRITE] WRITE ERROR!\r\n");
        return;
    }

    DBG("[WRITE] SUCCESS!\r\n");
}

static void Cmd_CalcCRC(void) {
    DBG("\r\n[CALCCRC] Calculate CRC32 mode\r\n");
    DBG("[CALCCRC] Enter start address (hex, e.g. 08000000): ");
    uint32_t addr = ReadHex32();

    DBG("[CALCCRC] Enter length in hex (e.g. 20000 = 128 KB): ");
    uint32_t len = ReadHex32();

    DBG("[CALCCRC] Calculating CRC32 for 0x");
    DBG_Hex(len, 8);
    DBG(" bytes from 0x");
    DBG_Hex(addr, 8);
    DBG("...\r\n");

    uint8_t buffer[256];
    uint32_t bytes_read = 0;
    uint32_t crc = 0xFFFFFFFF;
    const uint32_t poly = 0x04C11DB7;

    while (bytes_read < len) {
        uint16_t chunk = (len - bytes_read > 256) ? 256 : (uint16_t)(len - bytes_read);

        if (!BL_ReadMemory(addr + bytes_read, buffer, chunk)) {
            DBG("[CALCCRC] READ ERROR!\r\n");
            return;
        }

        for (uint16_t i = 0; i < chunk; i++) {
            crc ^= ((uint32_t)buffer[i] << 24);
            for (int j = 0; j < 8; j++) {
                if (crc & 0x80000000) {
                    crc = (crc << 1) ^ poly;
                } else {
                    crc <<= 1;
                }
            }
        }

        bytes_read += chunk;

        if (bytes_read % 8192 == 0) {
            char msg[64];
            sprintf(msg, "[CALCCRC] Progress: %lu / %lu bytes\r\n", bytes_read, len);
            DBG(msg);
        }
    }

    crc ^= 0xFFFFFFFF;

    DBG("[CALCCRC] CRC32 = 0x");
    DBG_Hex(crc, 8);
    DBG("\r\n");
    DBG("[CALCCRC] Done!\r\n");
}

static void Cmd_CheckCRC(void) {
    DBG("\r\n[CRC] CRC32 verification mode\r\n");
    DBG("[CRC] Enter start address (hex, e.g. 08000000): ");
    uint32_t addr = ReadHex32();

    DBG("[CRC] Enter length in hex (e.g. 20000 = 128 KB): ");
    uint32_t len = ReadHex32();

    DBG("[CRC] Enter expected CRC32 (hex, e.g. A1B2C3D4): ");
    uint32_t expected = ReadHex32();

    DBG("[CRC] Calculating CRC32 for 0x");
    DBG_Hex(len, 8);
    DBG(" bytes from 0x");
    DBG_Hex(addr, 8);
    DBG("...\r\n");
    DBG("[CRC] This may take a few seconds...\r\n");

    uint8_t buffer[256];
    uint32_t bytes_read = 0;
    uint32_t crc = 0xFFFFFFFF;
    const uint32_t poly = 0x04C11DB7;

    while (bytes_read < len) {
        uint16_t chunk = (len - bytes_read > 256) ? 256 : (uint16_t)(len - bytes_read);

        if (!BL_ReadMemory(addr + bytes_read, buffer, chunk)) {
            DBG("[CRC] READ ERROR during CRC calculation!\r\n");
            return;
        }

        for (uint16_t i = 0; i < chunk; i++) {
            crc ^= ((uint32_t)buffer[i] << 24);
            for (int j = 0; j < 8; j++) {
                if (crc & 0x80000000) {
                    crc = (crc << 1) ^ poly;
                } else {
                    crc <<= 1;
                }
            }
        }

        bytes_read += chunk;

        if (bytes_read % 8192 == 0) {
            char msg[64];
            sprintf(msg, "[CRC] Progress: %lu / %lu bytes\r\n", bytes_read, len);
            DBG(msg);
        }
    }

    crc ^= 0xFFFFFFFF;

    DBG("[CRC] Calculated CRC32: 0x");
    DBG_Hex(crc, 8);
    DBG("\r\n");

    DBG("[CRC] Expected CRC32:   0x");
    DBG_Hex(expected, 8);
    DBG("\r\n");

    if (crc == expected) {
        DBG("[CRC] ✅ MATCH! Firmware is valid.\r\n");
    } else {
        DBG("[CRC] ❌ MISMATCH! Firmware is corrupted.\r\n");
    }
}

static void Cmd_Reset(void) {
    DBG("\r\n[RESET] Resetting F103 to normal mode...\r\n");
    BOOT_Stop();
    DBG("[RESET] Done. F103 booting from Flash.\r\n");
    DBG("[RESET] To re-enter bootloader, restart programmer.\r\n");
}

static void Cmd_Help(void) {
    DBG("\r\n");
    DBG("========================================\r\n");
    DBG("      STM32 PROGRAMMER (UART BL)\r\n");
    DBG("========================================\r\n");
    DBG(" 1 - Info      : get chip version/PID\r\n");
    DBG(" 2 - Read      : read 256 bytes\r\n");
    DBG(" 3 - ReadAll   : read entire 128 KB\r\n");
    DBG(" 4 - Erase     : erase entire Flash\r\n");
    DBG(" 5 - Write     : write data\r\n");
    DBG(" 6 - CalcCRC   : calculate CRC32\r\n");
    DBG(" 7 - CheckCRC  : verify CRC32\r\n");
    DBG(" 8 - Reset     : reset F103 to normal\r\n");
    DBG(" ? - Help      : show this menu\r\n");
    DBG("========================================\r\n");
    DBG("IMPORTANT:\r\n");
    DBG("- Flash starts at 08000000\r\n");
    DBG("- Erase before Write!\r\n");
    DBG("- After Reset, restart to re-enter BL\r\n");
    DBG("========================================\r\n");
}

/* ========== MAIN FUNCTION ========== */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    DBG("\r\n========================================\r\n");
    DBG("  STM32 Programmer v1.2\r\n");
    DBG("========================================\r\n");
    DBG("[BOOT] Entering F103 bootloader...\r\n");
    BOOT_Start();

    DBG("[BOOT] Synchronizing...\r\n");
    if (!BL_Sync()) {
        DBG("[BOOT] ERROR! Bootloader not responding.\r\n");
        DBG("[BOOT] Check: TX/RX crossed, BOOT0=3.3V\r\n");
        while (1) { HAL_Delay(500); }
    }
    DBG("[BOOT] SUCCESS! Ready.\r\n");

    Cmd_Help();

    while (1) {
        DBG("\r\n> ");
        uint8_t cmd = 0;
        while (HAL_UART_Receive(&huart2, &cmd, 1, 100) != HAL_OK) { }

        switch (cmd) {
            case '1': Cmd_Info();    break;
            case '2': Cmd_Read();    break;
            case '3': Cmd_ReadAll(); break;
            case '4': Cmd_Erase();   break;
            case '5': Cmd_Write();   break;
            case '6': Cmd_CalcCRC(); break;
            case '7': Cmd_CheckCRC(); break;
            case '8': Cmd_Reset();   break;
            case '?': Cmd_Help();    break;
            default:
                DBG("\r\nUnknown command. Press '?' for help.\r\n");
                break;
        }
    }
}

/* ========== SYSTEM CLOCK ========== */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
        Error_Handler();

    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
}

/* ========== USART1 (to F103) ========== */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

/* ========== USART2 (debug/menu) ========== */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/* ========== GPIO ========== */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_RESET_F103, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_BOOT0, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_RESET_F103;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_BOOT0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* ========== ERROR HANDLER ========== */
void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
