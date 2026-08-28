/*
 * EEPROM.c
 *
 *  Created on: Aug 12, 2026
 *      Author: yaros
 */

#include "EEPROM.h"
#include "main.h"
#include <stdint.h>



extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_tx;


void EEPROM_write(uint8_t address_data,uint8_t address_device,uint8_t* data,uint8_t len){
	uint8_t data_recieve[256];
	data_recieve[0] = address_data;
	for(size_t i = 0; i<len;++i){
		data_recieve[i+1] = data[i];
	}
	HAL_I2C_Master_Transmit(&hi2c2, address_device, data_recieve, len+1, 100);
}
void EEPROM_Read(uint8_t address_data,uint8_t address_device,uint8_t *buffer,uint8_t len){

	HAL_I2C_Mem_Read(&hi2c2, address_device, address_data, I2C_MEMADD_SIZE_8BIT, buffer, len, 100);
}
