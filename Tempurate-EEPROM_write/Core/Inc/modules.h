/*
 * modules.h
 *
 *  Created on: Aug 9, 2026
 *      Author: yaros
 */

#ifndef INC_MODULES_H_
#define INC_MODULES_H_

#include "stdint.h"

#define INIT_DONE 0x01
#define TRANSMIT_DONE 0x02
#define RECIEVE_DONE 0x03
#define TRANSMIT_ERROR 0xFF
#define RECIEVE_ERROR 0xF0

typedef struct{
	uint32_t hum;
	uint32_t temp;
	uint8_t status; //статус нулевой
	uint8_t global_status;
} AHT20_Struct;

extern AHT20_Struct aht20_data;

void AHT_Init(uint8_t address_device);
void AHT_Command_Transmit(uint8_t address_device);
void AHT_Data_Recieve(uint8_t address_device, uint32_t* data_Humidity, uint32_t* data_Temperature);
#endif /* INC_MODULES_H_ */
