#include "modules.h"
#include "main.h"

extern UART_HandleTypeDef huart1;

/* ============================================
 * УПРАВЛЕНИЕ ЦЕЛЕВЫМ MCU
 * ============================================ */

void BOOT_Start(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_BOOT0, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_RESET_F103, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOB, GPIO_RESET_F103, GPIO_PIN_SET);
    HAL_Delay(500);
}

void BOOT_Stop(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_BOOT0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_RESET_F103, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOB, GPIO_RESET_F103, GPIO_PIN_SET);
}

/* ============================================
 * ПЕРЕКЛЮЧЕНИЕ РЕЖИМА UART
 * ============================================ */

static void UART_SetMode_8N1(void) {
    HAL_UART_DeInit(&huart1);
    huart1.Instance = USART1;
    huart1.Init.BaudRate   = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits   = UART_STOPBITS_1;
    huart1.Init.Parity     = UART_PARITY_NONE;
    huart1.Init.Mode       = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&huart1);
}

static void UART_SetMode_9B_Even(void) {
    HAL_UART_DeInit(&huart1);
    huart1.Instance = USART1;
    huart1.Init.BaudRate   = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_9B;
    huart1.Init.StopBits   = UART_STOPBITS_1;
    huart1.Init.Parity     = UART_PARITY_EVEN;
    huart1.Init.Mode       = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&huart1);
}

/* ============================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ============================================ */

static void UART_FlushRX(void) {
    uint8_t dummy;
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        HAL_UART_Receive(&huart1, &dummy, 1, 10);
    }
}

static uint8_t WaitACK(uint32_t timeout_ms) {
    uint8_t ack = 0;
    if (HAL_UART_Receive(&huart1, &ack, 1, timeout_ms) != HAL_OK) return 0;
    return (ack == 0x79) ? 1 : 0;
}

/* ============================================
 * СИНХРОНИЗАЦИЯ (8N1 → 0x7F → 9B+Even)
 * ============================================ */

uint8_t BL_Sync(void) {
    uint8_t sync_byte = 0x7F;

    UART_SetMode_8N1();
    HAL_Delay(10);

    for (int attempt = 0; attempt < 3; attempt++) {
        UART_FlushRX();
        if (HAL_UART_Transmit(&huart1, &sync_byte, 1, 100) != HAL_OK) continue;
        if (WaitACK(500)) {
            UART_SetMode_9B_Even();
            HAL_Delay(10);
            return 1;
        }
        HAL_Delay(200);
    }
    return 0;
}

/* ============================================
 * GET VERSION (0x01) — узнать версию и PID
 * ============================================ */

uint8_t BL_GetVersion(uint8_t *ver, uint8_t *pid) {
    uint8_t cmd[2] = {0x01, 0xFE};
    if (HAL_UART_Transmit(&huart1, cmd, 2, 100) != HAL_OK) return 0;
    if (!WaitACK(500)) return 0;

    if (HAL_UART_Receive(&huart1, ver, 1, 300) != HAL_OK) return 0;
    if (HAL_UART_Receive(&huart1, pid, 1, 300) != HAL_OK) return 0;
    if (!WaitACK(500)) return 0;

    return 1;
}

/* ============================================
 * READ MEMORY (0x11)
 * ============================================ */

uint8_t BL_ReadMemory(uint32_t address, uint8_t *buffer, uint16_t len) {
    if (len == 0 || len > 256) return 0;

    uint8_t cmd[2] = {0x11, 0xEE};
    if (HAL_UART_Transmit(&huart1, cmd, 2, 100) != HAL_OK) return 0;
    if (!WaitACK(500)) return 0;

    uint8_t addr[5];
    addr[0] = (address >> 24) & 0xFF;
    addr[1] = (address >> 16) & 0xFF;
    addr[2] = (address >>  8) & 0xFF;
    addr[3] =  address        & 0xFF;
    addr[4] = addr[0] ^ addr[1] ^ addr[2] ^ addr[3];
    if (HAL_UART_Transmit(&huart1, addr, 5, 100) != HAL_OK) return 0;
    if (!WaitACK(500)) return 0;

    uint8_t nb[2];
    nb[0] = len - 1;
    nb[1] = ~nb[0];
    if (HAL_UART_Transmit(&huart1, nb, 2, 100) != HAL_OK) return 0;
    if (!WaitACK(500)) return 0;

    if (HAL_UART_Receive(&huart1, buffer, len, 1000) != HAL_OK) return 0;
    return 1;
}

/* ============================================
 * ERASE MEMORY (0x44 — Extended Erase для F103)
 * ============================================ */

uint8_t BL_EraseMemory(void) {
    // Команда Extended Erase: 0x44 + 0xBB
    uint8_t cmd[2] = {0x44, 0xBB};
    if (HAL_UART_Transmit(&huart1, cmd, 2, 100) != HAL_OK) return 0;
    if (!WaitACK(1000)) return 0;

    // 0xFFFF = Mass Erase (стереть всю Flash)
    uint8_t mass_erase[3] = {0xFF, 0xFF, 0x00};  // 0xFF ^ 0xFF = 0x00
    if (HAL_UART_Transmit(&huart1, mass_erase, 3, 100) != HAL_OK) return 0;

    // Ждём ACK — стирание занимает 1-3 секунды!
    if (!WaitACK(5000)) return 0;

    return 1;
}

/* ============================================
 * WRITE MEMORY (0x31)
 * ============================================ */

uint8_t BL_WriteMemory(uint32_t address, const uint8_t *data, uint16_t len) {
    if (len == 0 || len > 256) return 0;

    // Команда Write Memory: 0x31 + 0xCE
    uint8_t cmd[2] = {0x31, 0xCE};
    if (HAL_UART_Transmit(&huart1, cmd, 2, 100) != HAL_OK) return 0;
    if (!WaitACK(500)) return 0;

    // Адрес (4 байта big-endian) + XOR
    uint8_t addr[5];
    addr[0] = (address >> 24) & 0xFF;
    addr[1] = (address >> 16) & 0xFF;
    addr[2] = (address >>  8) & 0xFF;
    addr[3] =  address        & 0xFF;
    addr[4] = addr[0] ^ addr[1] ^ addr[2] ^ addr[3];
    if (HAL_UART_Transmit(&huart1, addr, 5, 100) != HAL_OK) return 0;
    if (!WaitACK(500)) return 0;

    // N = количество байт - 1
    uint8_t n = len - 1;
    if (HAL_UART_Transmit(&huart1, &n, 1, 100) != HAL_OK) return 0;

    // Данные
    if (HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 500) != HAL_OK) return 0;

    // XOR всех байт: N ^ data[0] ^ data[1] ^ ... ^ data[len-1]
    uint8_t xor_val = n;
    for (uint16_t i = 0; i < len; i++) {
        xor_val ^= data[i];
    }
    if (HAL_UART_Transmit(&huart1, &xor_val, 1, 100) != HAL_OK) return 0;

    // Ждём ACK
    if (!WaitACK(1000)) return 0;

    return 1;
}
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint32_t poly = 0x04C11DB7;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= ((uint32_t)data[i] << 24);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;  // инверсия результата
}

/* ============================================
 * ПРОВЕРКА CRC ВСЕЙ FLASH ИЛИ ЧАСТИ
 * ============================================ */

uint8_t BL_VerifyCRC(uint32_t start_addr, uint32_t length, uint32_t expected_crc) {
    uint8_t buffer[256];
    uint32_t bytes_read = 0;
    uint32_t crc = 0xFFFFFFFF;
    const uint32_t poly = 0x04C11DB7;

    while (bytes_read < length) {
        uint16_t chunk = (length - bytes_read > 256) ? 256 : (uint16_t)(length - bytes_read);

        if (!BL_ReadMemory(start_addr + bytes_read, buffer, chunk)) {
            return 0;  // ошибка чтения
        }

        // Вычисляем CRC для этого блока
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
    }

    crc ^= 0xFFFFFFFF;  // инверсия

    return (crc == expected_crc) ? 1 : 0;
}
