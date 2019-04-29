#include "stm32f2xx.h"
#include "stm32f2xx_hal.h"
#include "stm32f2xx_hal_flash.h"
#include "stm32f2xx_hal_flash_ex.h"
#include "common/bootcmd.h"
#include <stdbool.h>
#include "string.h"

void jump_to_program();
void interact_bcmd(bool invalidate, uint8_t);
void write_bcmd(const bootcmd_t * p);

void WWDG_IRQHandler() {
	while (1) {
		;
	}
}

int main() {
	HAL_Init();
	interact_bcmd(false, 1);

	if (BCMD->cmd == BOOTCMD_RUN && BCMD->is_safe) {
		jump_to_program();

		while (1) {;}
	}
	if (BCMD->cmd != BOOTCMD_UPDATE && !BCMD->is_safe) {
		interact_bcmd(true, 1);
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
	
	uint32_t size = BCMD->size;

	if (size % 4 != 0) {
		size += (4 - (size % 4)); // round to next 4
	}
	
	for (uint32_t i = 0; i < size; i += 4) {
		HAL_FLASH_Program(TYPEPROGRAM_WORD, 0x08004000 + i, *(uint32_t *)(0x08080000 + i));
	}

	if (memcmp((const void *)(0x08004000), (const void *)(0x08080000), BCMD->size) != 0) {
		// UHOH
		HAL_FLASH_Lock();
		NVIC_SystemReset();
	}
	

	// verify is ok, we can now erase the update partition and mark everything as hunkidory.
	// this also means that a program can check a 
	
	bootcmd_t new_bcm = {
		.signature = {0xae, 0x7d},
		.cmd = BOOTCMD_RUN,
		.size = 0,
		.is_safe = 1,
		.current_size = BCMD->size
	};

	write_bcmd(&new_bcm);
	HAL_FLASH_Lock();

	// the system is now updated, reset
	NVIC_SystemReset();
}

void jump_to_program() {

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	__set_MSP(*(uint32_t *)(0x08004000));

	uint32_t address = *(uint32_t *)(0x08004004);
	( (void ( * )( void ) ) address ) ();
}

void write_bcmd(const bootcmd_t * p) {
	HAL_FLASH_Unlock();

	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
	FLASH_Erase_Sector(FLASH_SECTOR_11, FLASH_VOLTAGE_RANGE_3);
	
	for (uint32_t i = 0; i < sizeof(bootcmd_t); ++i) {
		if (HAL_FLASH_Program(TYPEPROGRAM_BYTE, BCMD_BASE + i, *(((uint8_t *)(p)) + i)) != HAL_OK) {
			// uhoh
			
			HAL_FLASH_Lock();
			NVIC_SystemReset();
		}
	}

	HAL_FLASH_Lock();
}

void interact_bcmd(bool invalidate, uint8_t safe) {
	if (BCMD->signature[0] != 0xae || BCMD->signature[1] != 0x7d || invalidate) {
		// overwrite the BMCD structure

		
		bootcmd_t new_bcm = {
			.signature = {0xae, 0x7d},
			.cmd = BOOTCMD_RUN,
			.size = 0,
			.is_safe = safe,
			.current_size = 0
		};

		write_bcmd(&new_bcm);
		NVIC_SystemReset();
	}
}
