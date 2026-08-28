/*
 * modules.c
 *
 *  Created on: Aug 9, 2026
 *      Author: yaros
 */
#include "modules.h"
#include "main.h"
#include "stdint.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_tx;

AHT20_Struct aht20_data = {0};

void AHT_Init(uint8_t address_device){
	uint8_t push_command = 0x71;
	HAL_Delay(100);
	HAL_I2C_Master_Transmit(&hi2c1, address_device, &push_command, 1, 100);
	HAL_I2C_Master_Receive(&hi2c1, address_device, &aht20_data.status, 1, 100);
	if(aht20_data.status == 0x18){
		aht20_data.global_status = INIT_DONE;
	}


}
void AHT_Command_Transmit(uint8_t address_device){
	HAL_Delay(10);
	uint8_t cmd[3] = {0xAC,0x33,0x00};
	HAL_I2C_Master_Transmit(&hi2c1, address_device, cmd, 3, 100);
	HAL_Delay(80);
	HAL_I2C_Master_Receive(&hi2c1, address_device, &aht20_data.status, 1, 100);
	uint8_t attemps = 0;
	while((aht20_data.status & (1<<7))!=0){
		HAL_UART_Transmit(&huart1, (uint8_t*)("WAIT\n\r"), 6, 100);
		HAL_I2C_Master_Receive(&hi2c1, address_device, &aht20_data.status, 1, 100);
		HAL_Delay(80);
		attemps++;
		if(attemps>=5){
			aht20_data.global_status = TRANSMIT_ERROR;
			return;  //выходим из функции чтобы не было бесконечного цикла
		}
	}

	HAL_UART_Transmit(&huart1, (uint8_t*)("DONE\n\r"), 6, 100);
	aht20_data.global_status = TRANSMIT_DONE;

}

void AHT_Data_Recieve(uint8_t address_device, uint32_t* data_Humidity, uint32_t* data_Temperature){
	uint8_t data_recieve[6];
	HAL_I2C_Master_Receive(&hi2c1, address_device | 1, data_recieve, 6, 100);
	*data_Humidity = data_recieve[1]<<12 | data_recieve[2]<<4 | (data_recieve[3] >>4) ; //получаем влажность
	 *data_Temperature = (data_recieve[3]& 0xF)<<12 | data_recieve[4]<<4 | (data_recieve[5] ); //получаем температуру
	 if(*data_Humidity == 0x00 || *data_Temperature == 0x00){
		 aht20_data.global_status = RECIEVE_ERROR; // данные нулевые == ошибка
		 return ;
	 }
	 aht20_data.global_status = RECIEVE_DONE;

}
