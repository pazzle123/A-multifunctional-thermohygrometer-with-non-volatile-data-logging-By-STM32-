/*
 * EEPROM.h
 *
 *  Created on: Aug 12, 2026
 *      Author: yaros
 */

#ifndef INC_EEPROM_H_
#define INC_EEPROM_H_

#include <stdint.h>


void EEPROM_write(uint8_t address_data,uint8_t address_device,uint8_t* data,uint8_t len);
void EEPROM_Read(uint8_t address_data,uint8_t address_device,uint8_t *buffer,uint8_t len);

#endif /* INC_EEPROM_H_ */
