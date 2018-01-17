/*
 * xdk_tests.c
 *
 *  Created on: Jan 17, 2018
 *      Author: jan
 */


/** See "FreeRTOS Guide XDK110" chapter 4.1 for information on the internal Flash */

#include "BCDS_MCU_Flash.h"

void readTestFromFlash(void) {
	 uint32_t readAddress = 0x000B6000;

	 uint8_t buffer[4];
	 uint32_t length = sizeof(buffer);
	 MCU_Flash_Read((uint8_t *) readAddress, buffer, length);
	 printf("Memory: %d, %d, %d, %d\n\r", buffer[0], buffer[1], buffer[2], buffer[3]);
}

void writeTestToFlash(void) {
	 uint32_t writeAddress = 0x000B6000;
	 uint8_t buffer[4] = {1,2,4,16};
	 uint32_t length = sizeof(buffer);
	 MCU_Flash_Write((uint8_t *) writeAddress, buffer, length);
	 memset(buffer, 0, 4);
	 MCU_Flash_Read((uint8_t *) writeAddress, buffer, length);
	 printf("Blaa! Memory: %d, %d, %d, %d, page sz = %d\n\r", buffer[0], buffer[1], buffer[2],
			 buffer[3], MCU_Flash_GetPageSize());
}
