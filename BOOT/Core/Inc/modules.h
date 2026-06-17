#ifndef INC_MODULES_H_
#define INC_MODULES_H_

#include "stdint.h"

#define GPIO_BOOT0        GPIO_PIN_8
#define GPIO_RESET_F103   GPIO_PIN_0

// Управление MCU
void    BOOT_Start(void);
void    BOOT_Stop(void);

// Синхронизация
uint8_t BL_Sync(void);

// Команды загрузчика
uint8_t BL_GetVersion(uint8_t *ver, uint8_t *pid);
uint8_t BL_ReadMemory(uint32_t address, uint8_t *buffer, uint16_t len);
uint8_t BL_EraseMemory(void);
uint8_t BL_WriteMemory(uint32_t address, const uint8_t *data, uint16_t len);

// CRC-проверка
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length);
uint8_t  BL_VerifyCRC(uint32_t start_addr, uint32_t length, uint32_t expected_crc);

#endif
