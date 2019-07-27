#include "stm32f2xx.h"
#include "stm32f2xx_hal.h"
#include "stm32f2xx_hal_flash.h"
#include "stm32f2xx_hal_flash_ex.h"
#include "common/bootcmd.h"
#include <stdbool.h>
#include "string.h"

void jump_to_program();

int main() {
	HAL_Init();
	bootcmd_init();

	if (bootcmd_get_cmd() == BOOTCMD_RUN) {
		jump_to_program();

		while (1) {;}
	}
	if (bootcmd_get_cmd() != BOOTCMD_UPDATE) {
		while (1) {;}
	}

	// Alright, now we can read the BCMD to figure out what to do

	// Step 1:
	// (all happens in one go)
	// Erase the flash
	FLASH_EraseInitTypeDef erase_typedef = {0};
	erase_typedef.TypeErase = TYPEERASE_SECTORS;
	erase_typedef.Sector = FLASH_SECTOR_1;
	erase_typedef.NbSectors = 8;
	erase_typedef.VoltageRange = FLASH_VOLTAGE_RANGE_3;

	uint32_t sector_error;

	HAL_FLASH_Unlock();

	if (HAL_FLASHEx_Erase(&erase_typedef, &sector_error) != HAL_OK) {
		// oh no
	}

	// Step 3: Copy flash, rounding up to take full advantage.
	
	uint32_t size = bootcmd_update_size();

	if (size % 4 != 0) {
		size += (4 - (size % 4)); // round to next 4
	}
	
	for (uint32_t i = 0; i < size; i += 4) {
		HAL_FLASH_Program(TYPEPROGRAM_WORD, 0x08004000 + i, *(uint32_t *)(0x08080000 + i));
	}

	if (memcmp((const void *)(0x08004000), (const void *)(0x08080000), bootcmd_update_size()) != 0) {
		// UHOH
		HAL_FLASH_Lock();
		NVIC_SystemReset();
	}
	

	// verify is ok, we can now erase the update partition and mark everything as hunkidory.
	// this also means that a program can check a 
	
	HAL_FLASH_Lock();
	bootcmd_service_update();

	// the system is now updated, reset
	NVIC_SystemReset();
}

void jump_to_program() {

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	__set_MSP(*(uint32_t *)(0x08004000));

	((void(*)(void))(*(uint32_t *)(0x08004004)))();
}
